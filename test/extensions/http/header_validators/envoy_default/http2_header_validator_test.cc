#include "source/extensions/http/header_validators/envoy_default/http2_header_validator.h"

#include "test/extensions/http/header_validators/envoy_default/header_validator_test.h"

namespace Envoy {
namespace Extensions {
namespace Http {
namespace HeaderValidators {
namespace EnvoyDefault {
namespace {

using ::Envoy::Extensions::Http::HeaderValidators::EnvoyDefault::Http2HeaderValidator;
using ::Envoy::Http::HeaderString;
using ::Envoy::Http::HeaderValidator;

class Http2HeaderValidatorTest : public HeaderValidatorTest {
protected:
  Http2HeaderValidatorPtr createH2(absl::string_view config_yaml) {
    envoy::extensions::http::header_validators::envoy_default::v3::HeaderValidatorConfig
        typed_config;
    TestUtility::loadFromYaml(std::string(config_yaml), typed_config);

    return std::make_unique<Http2HeaderValidator>(typed_config, stream_info_);
  }
};

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapAllowed) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{{":scheme", "https"},
                                                  {":method", "GET"},
                                                  {":path", "/"},
                                                  {":authority", "envoy.com"},
                                                  {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapMissingPath) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":scheme", "https"}, {":method", "GET"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapMissingMethod) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":scheme", "https"}, {":path", "/"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapMissingScheme) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":path", "/"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapExtraPseudoHeader) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":scheme", "https"}, {":method", "GET"}, {":path", "/"}, {":foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapConnect) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":method", "CONNECT"}, {":authority", "envoy.com"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapConnectExtraPseudoHeader) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{
      {":method", "CONNECT"}, {":scheme", "https"}, {":authority", "envoy.com"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMapInvalidAuthority) {
  ::Envoy::Http::TestRequestHeaderMapImpl headers{{":scheme", "https"},
                                                  {":method", "GET"},
                                                  {":path", "/"},
                                                  {":authority", "user:pass@envoy.com"},
                                                  {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderMap(headers),
            HeaderValidator::RequestHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderMapValid) {
  ::Envoy::Http::TestResponseHeaderMapImpl headers{{":status", "200"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateResponseHeaderMap(headers),
            HeaderValidator::ResponseHeaderMapValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderMapMissingStatus) {
  ::Envoy::Http::TestResponseHeaderMapImpl headers{{"x-foo", "bar"}};
  auto uhv = createH2(empty_config);
  EXPECT_EQ(uhv->validateResponseHeaderMap(headers),
            HeaderValidator::ResponseHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderMapExtraPseudoHeader) {
  ::Envoy::Http::TestResponseHeaderMapImpl headers{
      {":status", "200"}, {":foo", "bar"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);
  EXPECT_EQ(uhv->validateResponseHeaderMap(headers),
            HeaderValidator::ResponseHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderMapInvalidStatus) {
  ::Envoy::Http::TestResponseHeaderMapImpl headers{{":status", "1024"}, {"x-foo", "bar"}};
  auto uhv = createH2(empty_config);
  EXPECT_EQ(uhv->validateResponseHeaderMap(headers),
            HeaderValidator::ResponseHeaderMapValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateTransferEncoding) {
  HeaderString trailers{"trailers"};
  HeaderString chunked{"chunked"};
  auto uhv = createH2(empty_config);
  EXPECT_EQ(uhv->validateTransferEncodingHeader(trailers),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateTransferEncodingHeader(chunked),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateAuthority) {
  HeaderString valid{"envoy.com:443"};
  HeaderString valid_no_port{"envoy.com"};
  HeaderString invalid_empty{""};
  HeaderString invalid_userinfo{"foo:bar@envoy.com"};
  HeaderString invalid_port_int{"envoy.com:a"};
  HeaderString invalid_port_trailer{"envoy.com:10a"};
  HeaderString invalid_port_value{"envoy.com:66000"};
  HeaderString invalid_port_0{"envoy.com:0"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateAuthorityHeader(valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateAuthorityHeader(valid_no_port),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_empty),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_userinfo),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_port_int),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_port_trailer),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_port_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateAuthorityHeader(invalid_port_0),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidatePath) {
  HeaderString valid{"/"};
  auto uhv = createH2(empty_config);
  // TODO(meilya) - after path normalization has been approved and implemented
  EXPECT_EQ(uhv->validatePathHeader(valid), HeaderValidator::HeaderEntryValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateGenericHeaderKeyConnectionRejected) {
  HeaderString transfer_encoding{"transfer-encoding"};
  HeaderString connection{"connection"};
  HeaderString keep_alive{"keep-alive"};
  HeaderString upgrade{"upgrade"};
  HeaderString proxy_connection{"proxy-connection"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateGenericHeaderName(transfer_encoding),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(connection),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(keep_alive),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(upgrade),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateGenericHeaderName(proxy_connection),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

/* TODO(meilya) - add generic header name validation here */

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderAuthority) {
  HeaderString authority{":authority"};
  HeaderString valid{"envoy.com"};
  HeaderString invalid{"user:pass@envoy.com"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(authority, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(authority, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderAuthorityHost) {
  HeaderString host{"host"};
  HeaderString valid{"envoy.com"};
  HeaderString invalid{"user:pass@envoy.com"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(host, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(host, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderPath) {
  HeaderString path{":path"};
  HeaderString valid{"/"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(path, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  // TODO(meilya) - add invalid case when path normalization is ready
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderTransferEncoding) {
  HeaderString transfer_encoding{"TE"};
  HeaderString valid{"trailers"};
  HeaderString invalid{"chunked"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(transfer_encoding, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(transfer_encoding, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMethodAllowAllMethods) {
  HeaderString method{":method"};
  HeaderString valid{"GET"};
  HeaderString invalid{"CUSTOM-METHOD"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(method, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(method, invalid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderMethodRestrictMethods) {
  HeaderString method{":method"};
  HeaderString valid{"GET"};
  HeaderString invalid{"CUSTOM-METHOD"};
  auto uhv = createH2(restrict_http_methods_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(method, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(method, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderContentLength) {
  HeaderString content_length{"content-length"};
  HeaderString valid{"100"};
  HeaderString invalid{"10a2"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(content_length, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(content_length, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderScheme) {
  HeaderString scheme{":scheme"};
  HeaderString valid{"https"};
  HeaderString invalid{"http_ssh"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(scheme, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(scheme, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderGeneric) {
  HeaderString valid_name{"x-foo"};
  HeaderString invalid_name{""};
  HeaderString valid_value{"bar"};
  HeaderString invalid_value;
  auto uhv = createH2(empty_config);

  setHeaderStringUnvalidated(invalid_value, "hello\nworld");

  EXPECT_EQ(uhv->validateRequestHeaderEntry(valid_name, valid_value),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(invalid_name, valid_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateRequestHeaderEntry(valid_name, invalid_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderAllowUnderscores) {
  HeaderString name{"x_foo"};
  HeaderString value{"bar"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(name, value),
            HeaderValidator::HeaderEntryValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateRequestHeaderRejectUnderscores) {
  HeaderString name{"x_foo"};
  HeaderString value{"bar"};
  auto uhv = createH2(reject_headers_with_underscores_config);

  EXPECT_EQ(uhv->validateRequestHeaderEntry(name, value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderStatus) {
  HeaderString status{":status"};
  HeaderString valid{"200"};
  HeaderString invalid{"1024"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateResponseHeaderEntry(status, valid),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateResponseHeaderEntry(status, invalid),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderGeneric) {
  HeaderString valid_name{"x-foo"};
  HeaderString invalid_name{""};
  HeaderString valid_value{"bar"};
  HeaderString invalid_value;
  auto uhv = createH2(empty_config);

  setHeaderStringUnvalidated(invalid_value, "hello\nworld");

  EXPECT_EQ(uhv->validateResponseHeaderEntry(valid_name, valid_value),
            HeaderValidator::HeaderEntryValidationResult::Accept);
  EXPECT_EQ(uhv->validateResponseHeaderEntry(invalid_name, valid_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
  EXPECT_EQ(uhv->validateResponseHeaderEntry(valid_name, invalid_value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderAllowUnderscores) {
  HeaderString name{"x_foo"};
  HeaderString value{"bar"};
  auto uhv = createH2(empty_config);

  EXPECT_EQ(uhv->validateResponseHeaderEntry(name, value),
            HeaderValidator::HeaderEntryValidationResult::Accept);
}

TEST_F(Http2HeaderValidatorTest, ValidateResponseHeaderRejectUnderscores) {
  HeaderString name{"x_foo"};
  HeaderString value{"bar"};
  auto uhv = createH2(reject_headers_with_underscores_config);

  EXPECT_EQ(uhv->validateResponseHeaderEntry(name, value),
            HeaderValidator::HeaderEntryValidationResult::Reject);
}

} // namespace
} // namespace EnvoyDefault
} // namespace HeaderValidators
} // namespace Http
} // namespace Extensions
} // namespace Envoy
