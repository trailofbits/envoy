#pragma once

#include "source/extensions/http/header_validators/envoy_default/http_header_validator.h"

namespace Envoy {
namespace Extensions {
namespace Http {
namespace HeaderValidators {
namespace EnvoyDefault {

class Http1HeaderValidator : public HttpHeaderValidator {
public:
  Http1HeaderValidator(
      const envoy::extensions::http::header_validators::envoy_default::v3::HeaderValidatorConfig&
          config,
      StreamInfo::StreamInfo& stream_info);

  HeaderEntryValidationResult
  validateRequestHeaderEntry(const ::Envoy::Http::HeaderString& key,
                             const ::Envoy::Http::HeaderString& value) override;

  HeaderEntryValidationResult
  validateResponseHeaderEntry(const ::Envoy::Http::HeaderString& key,
                              const ::Envoy::Http::HeaderString& value) override;

  RequestHeaderMapValidationResult
  validateRequestHeaderMap(::Envoy::Http::RequestHeaderMap& header_map) override;

  ResponseHeaderMapValidationResult
  validateResponseHeaderMap(::Envoy::Http::ResponseHeaderMap& header_map) override;

  // Validates the given transfer encoding header value
  HeaderEntryValidationResult
  validateTransferEncodingHeader(const ::Envoy::Http::HeaderString& value);

  // Validates the given path pseudo header value
  HeaderEntryValidationResult validatePathHeader(const ::Envoy::Http::HeaderString& value);
};

using Http1HeaderValidatorPtr = std::unique_ptr<Http1HeaderValidator>;

} // namespace EnvoyDefault
} // namespace HeaderValidators
} // namespace Http
} // namespace Extensions
} // namespace Envoy
