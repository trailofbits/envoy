#include "test/extensions/http/header_validators/envoy_default/header_validator_test.h"

#include "source/extensions/http/header_validators/envoy_default/http_header_validator.h"

namespace Envoy {
namespace Extensions {
namespace Http {
namespace HeaderValidators {
namespace EnvoyDefault {
namespace {

using ::Envoy::Http::HeaderString;
using ::Envoy::Http::HeaderValidator;

class BaseHttpHeaderValidator : public HttpHeaderValidator {
public:
  BaseHttpHeaderValidator(
      const envoy::extensions::http::header_validators::envoy_default::v3::HeaderValidatorConfig&
          config,
      StreamInfo::StreamInfo& stream_info)
      : HttpHeaderValidator(config, stream_info) {}

  virtual HeaderEntryValidationResult
  validateRequestHeaderEntry(const ::Envoy::Http::HeaderString&,
                             const ::Envoy::Http::HeaderString&) override {
    return HeaderEntryValidationResult::Accept;
  }

  virtual HeaderEntryValidationResult
  validateResponseHeaderEntry(const ::Envoy::Http::HeaderString&,
                              const ::Envoy::Http::HeaderString&) override {
    return HeaderEntryValidationResult::Accept;
  }

  virtual RequestHeaderMapValidationResult
  validateRequestHeaderMap(::Envoy::Http::RequestHeaderMap&) override {
    return RequestHeaderMapValidationResult::Accept;
  }

  virtual ResponseHeaderMapValidationResult
  validateResponseHeaderMap(::Envoy::Http::ResponseHeaderMap&) override {
    return ResponseHeaderMapValidationResult::Accept;
  }
};

using BaseHttpHeaderValidatorPtr = std::unique_ptr<BaseHttpHeaderValidator>;

class BaseHeaderValidatorTest : public HeaderValidatorTest {
protected:
  BaseHttpHeaderValidatorPtr createBase(absl::string_view config_yaml) {
    envoy::extensions::http::header_validators::envoy_default::v3::HeaderValidatorConfig
        typed_config;
    TestUtility::loadFromYaml(std::string(config_yaml), typed_config);

    return std::make_unique<BaseHttpHeaderValidator>(typed_config, stream_info_);
  }
};

TEST_F(BaseHeaderValidatorTest, ValidateMethodPermissive) {
  HeaderString valid{"GET"};
  HeaderString custom{"CUSTOM-METHOD"};
  auto uhv = createBase(empty_config);
  EXPECT_EQ(uhv->validateMethodHeader(valid), HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateMethodHeader(custom),
            HeaderValidator::HeaderEntryValidationResult::Accept);
}

TEST_F(BaseHeaderValidatorTest, ValidateMethodStrict) {
  HeaderString valid{"GET"};
  HeaderString custom{"CUSTOM-METHOD"};
  auto uhv = createBase(restrict_http_methods_config);
  EXPECT_EQ(uhv->validateMethodHeader(valid), HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateMethodHeader(custom),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateSchemeStrict) {
  auto mode = HttpHeaderValidator::SchemePseudoHeaderValidationMode::Strict;
  HeaderString valid{"https"};
  HeaderString invalid_first{"Https"};
  HeaderString invalid_middle{"http+Ssh"};
  auto uhv = createBase(empty_config);
  EXPECT_EQ(uhv->validateSchemeHeader(mode, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateSchemeHeader(mode, invalid_first),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateSchemeHeader(mode, invalid_middle),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateSchemeUppercase) {
  auto mode = HttpHeaderValidator::SchemePseudoHeaderValidationMode::AllowUppercase;
  HeaderString valid{"HTTPS"};
  HeaderString invalid_middle{"http_ssh"};
  auto uhv = createBase(empty_config);
  EXPECT_EQ(uhv->validateSchemeHeader(mode, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateSchemeHeader(mode, invalid_middle),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateResponseStatusNone) {
  auto mode = HttpHeaderValidator::StatusPseudoHeaderValidationMode::None;
  HeaderString valid{"200"};
  HeaderString valid_outside_of_range{"1024"};
  HeaderString invalid{"asdf"};
  auto uhv = createBase(empty_config);

  EXPECT_EQ(uhv->validateStatusHeader(mode, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateStatusHeader(mode, valid_outside_of_range),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateStatusHeader(mode, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateResponseStatusRange) {
  auto mode = HttpHeaderValidator::StatusPseudoHeaderValidationMode::ValueRange;
  HeaderString valid{"200"};
  HeaderString invalid_max{"1024"};
  HeaderString invalid_min{"99"};
  auto uhv = createBase(empty_config);

  EXPECT_EQ(uhv->validateStatusHeader(mode, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateStatusHeader(mode, invalid_max),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateStatusHeader(mode, invalid_min),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

// TODO(meilya) should we test validating response status with AllowKnownValues and Strict modes?
// this may be out of scope.

TEST_F(BaseHeaderValidatorTest, ValidateGenericHeaderKeyRejectUnderscores) {
  HeaderString valid{"x-foo"};
  HeaderString invalid_underscore{"x_foo"};
  HeaderString invalid_eascii{"x-foo\x80"};
  auto uhv = createBase(reject_headers_with_underscores_config);

  EXPECT_EQ(uhv->validateGenericHeaderName(valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateGenericHeaderName(invalid_underscore),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(invalid_eascii),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateGenericHeaderKeyStrict) {
  HeaderString valid{"x-foo"};
  HeaderString valid_underscore{"x_foo"};
  HeaderString invalid_eascii{"x-foo\x80"};
  HeaderString invalid_empty{""};
  auto uhv = createBase(empty_config);

  EXPECT_EQ(uhv->validateGenericHeaderName(valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateGenericHeaderName(valid_underscore),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateGenericHeaderName(invalid_eascii),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(invalid_empty),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateGenericHeaderValue) {
  HeaderString valid{"hello world"};
  HeaderString valid_eascii{"value\x80"};
  HeaderString invalid_newline;
  auto uhv = createBase(empty_config);

  setHeaderStringUnvalidated(invalid_newline, "hello\nworld");

  EXPECT_EQ(uhv->validateGenericHeaderValue(valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateGenericHeaderValue(valid_eascii),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateGenericHeaderValue(invalid_newline),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateContentLength) {
  HeaderString valid{"100"};
  HeaderString invalid{"10a2"};
  auto uhv = createBase(empty_config);

  EXPECT_EQ(uhv->validateContentLengthHeader(valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateContentLengthHeader(invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(BaseHeaderValidatorTest, ValidateHostHeader) {
  HeaderString valid{"envoy.com:443"};
  HeaderString valid_no_port{"envoy.com"};
  HeaderString invalid_empty{""};
  HeaderString invalid_userinfo{"foo:bar@envoy.com"};
  HeaderString invalid_port_int{"envoy.com:a"};
  HeaderString invalid_port_trailer{"envoy.com:10a"};
  HeaderString invalid_port_value{"envoy.com:66000"};
  HeaderString invalid_port_0{"envoy.com:0"};
  auto uhv = createBase(empty_config);

  EXPECT_EQ(uhv->validateHostHeader(valid), HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateHostHeader(valid_no_port),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateHostHeader(invalid_empty),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateHostHeader(invalid_userinfo),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateHostHeader(invalid_port_int),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateHostHeader(invalid_port_trailer),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateHostHeader(invalid_port_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateHostHeader(invalid_port_0),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

} // namespace
} // namespace EnvoyDefault
} // namespace HeaderValidators
} // namespace Http
} // namespace Extensions
} // namespace Envoy
