#include "source/extensions/http/header_validators/envoy_default/header_validator.h"

#include <charconv>

#include "source/extensions/http/header_validators/envoy_default/character_tables.h"

#include "absl/container/node_hash_set.h"

namespace Envoy {
namespace Extensions {
namespace Http {
namespace HeaderValidators {
namespace EnvoyDefault {

using ::envoy::extensions::http::header_validators::envoy_default::v3::HeaderValidatorConfig;
using ::Envoy::Http::HeaderString;
using ::Envoy::Http::Protocol;

HeaderValidator::HeaderValidator(const HeaderValidatorConfig& config, Protocol protocol,
                                 StreamInfo::StreamInfo& stream_info)
    : config_(config), protocol_(protocol), stream_info_(stream_info),
      header_values_(::Envoy::Http::Headers::get()) {}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateMethodHeader(const HeaderString& value) {
  // HTTP Method Registry, from iana.org:
  // source: https://www.iana.org/assignments/http-methods/http-methods.xhtml
  //
  // From the RFC:
  //
  // tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "."
  //       /  "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
  // token = 1*tchar
  // method = token
  //
  static absl::node_hash_set<absl::string_view> kHttpMethodRegistry = {
      "ACL",
      "BASELINE-CONTROL",
      "BIND",
      "CHECKIN",
      "CHECKOUT",
      "CONNECT",
      "COPY",
      "DELETE",
      "GET",
      "HEAD",
      "LABEL",
      "LINK",
      "LOCK",
      "MERGE",
      "MKACTIVITY",
      "MKCALENDAR",
      "MKCOL",
      "MKREDIRECTREF",
      "MKWORKSPACE",
      "MOVE",
      "OPTIONS",
      "ORDERPATCH",
      "PATCH",
      "POST",
      "PRI",
      "PROPFIND",
      "PROPPATCH",
      "PUT",
      "REBIND",
      "REPORT",
      "SEARCH",
      "TRACE",
      "UNBIND",
      "UNCHECKOUT",
      "UNLINK",
      "UNLOCK",
      "UPDATE",
      "UPDATEREDIRECTREF",
      "VERSION-CONTROL",
      "*",
  };

  const auto& method = value.getStringView();
  bool is_valid = true;

  if (config_.restrict_http_methods()) {
    is_valid = kHttpMethodRegistry.contains(method);
  } else {
    is_valid = !method.empty();
    for (std::size_t i = 0; i < method.size() && is_valid; ++i) {
      is_valid = testChar(kMethodHeaderCharTable, method.at(i));
    }
  }

  if (!is_valid) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidMethod};
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateSchemeHeader(const HeaderString& value) {
  //
  // From RFC 3986, https://datatracker.ietf.org/doc/html/rfc3986#section-3.1:
  //
  // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  //
  // Although schemes are case-insensitive, the canonical form is lowercase and documents that
  // specify schemes must do so with lowercase letters. An implementation should accept uppercase
  // letters as equivalent to lowercase in scheme names (e.g., allow "HTTP" as well as "http") for
  // the sake of robustness but should only produce lowercase scheme names for consistency.
  //
  // The validation mode controls whether uppercase letters are permitted.
  //
  const auto& value_string_view = value.getStringView();

  if (value_string_view.empty()) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidScheme};
  }

  auto character_it = value_string_view.begin();

  // The first character must be an ALPHA
  auto valid_first_character = (*character_it >= 'a' && *character_it <= 'z') ||
                               (*character_it >= 'A' && *character_it <= 'Z');
  if (!valid_first_character) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidScheme};
  }

  for (++character_it; character_it != value_string_view.end(); ++character_it) {
    if (!testChar(kSchemeHeaderCharTable, *character_it)) {
      return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidScheme};
    }
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateStatusHeader(const StatusPseudoHeaderValidationMode& mode,
                                      const HeaderString& value) {
  //
  // This is based on RFC 7231, https://datatracker.ietf.org/doc/html/rfc7231#section-6,
  // describing the list of response status codes and the list of registered response status codes,
  // https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml.
  //
  static const absl::node_hash_set<std::uint32_t> kOfficialStatusCodes = {
      100, 102, 103, 200, 201, 202, 203, 204, 205, 206, 207, 208, 226, 300, 301, 302,
      303, 304, 305, 306, 307, 308, 400, 401, 402, 403, 404, 405, 406, 407, 408, 409,
      410, 411, 412, 413, 414, 415, 416, 417, 418, 421, 422, 423, 424, 425, 426, 428,
      429, 431, 451, 500, 501, 502, 503, 504, 505, 506, 507, 508, 510, 511,
  };
  static const uint32_t kMinimumResponseStatusCode = 100;
  static const uint32_t kMaximumResponseStatusCode = 599;

  const auto& value_string_view = value.getStringView();

  auto buffer_start = value_string_view.data();
  auto buffer_end = buffer_start + value_string_view.size();

  // Convert the status to an integer.
  std::uint32_t status_value{};
  auto result = std::from_chars(buffer_start, buffer_end, status_value);
  if (result.ec == std::errc::invalid_argument || result.ptr != buffer_end) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidStatus};
  }

  bool is_valid = false;

  switch (mode) {
  case StatusPseudoHeaderValidationMode::WholeNumber:
    is_valid = true;
    break;

  case StatusPseudoHeaderValidationMode::ValueRange:
    is_valid =
        status_value >= kMinimumResponseStatusCode && status_value <= kMaximumResponseStatusCode;
    break;

  case StatusPseudoHeaderValidationMode::OfficialStatusCodes:
    is_valid = kOfficialStatusCodes.contains(status_value);
    break;

  default:
    break;
  }

  if (!is_valid) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidStatus};
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateGenericHeaderName(const HeaderString& name) {
  //
  // Verify that the header name is valid. This also honors the underscore in
  // header configuration setting.
  //
  // From RFC 7230, https://datatracker.ietf.org/doc/html/rfc7230:
  //
  // header-field   = field-name ":" OWS field-value OWS
  // field-name     = token
  // token          = 1*tchar
  //
  // tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
  //                / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
  //                / DIGIT / ALPHA
  //                ; any VCHAR, except delimiters
  //
  const auto& key_string_view = name.getStringView();
  bool allow_underscores = !config_.reject_headers_with_underscores();
  // This header name is initially invalid if the name is empty.
  if (key_string_view.empty()) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().EmptyHeaderName};
  }

  bool is_valid = true;
  char c = '\0';

  for (std::size_t i{0}; i < key_string_view.size() && is_valid; ++i) {
    c = key_string_view.at(i);
    is_valid = testChar(kGenericHeaderNameCharTable, c) && (c != '_' || allow_underscores);
  }

  if (!is_valid) {
    auto details = c == '_' ? UhvResponseCodeDetail::get().InvalidUnderscore
                            : UhvResponseCodeDetail::get().InvalidCharacters;
    return {RejectAction::Reject, details};
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateGenericHeaderValue(const HeaderString& value) {
  //
  // Verify that the header value is valid.
  //
  // From RFC 7230, https://datatracker.ietf.org/doc/html/rfc7230:
  //
  // header-field   = field-name ":" OWS field-value OWS
  // field-value    = *( field-content / obs-fold )
  // field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
  // field-vchar    = VCHAR / obs-text
  // obs-text       = %x80-FF
  //
  // VCHAR          =  %x21-7E
  //                   ; visible (printing) characters
  //
  const auto& value_string_view = value.getStringView();
  bool is_valid = true;

  for (std::size_t i{0}; i < value_string_view.size() && is_valid; ++i) {
    is_valid = testChar(kGenericHeaderValueCharTable, value_string_view.at(i));
  }

  if (!is_valid) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidCharacters};
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateContentLengthHeader(const HeaderString& value) {
  //
  // From RFC 7230, https://datatracker.ietf.org/doc/html/rfc7230#section-3.3.2:
  //
  // Content-Length = 1*DIGIT
  //
  const auto& value_string_view = value.getStringView();

  if (value_string_view.empty()) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidContentLength};
  }

  auto buffer_start = value_string_view.data();
  auto buffer_end = buffer_start + value_string_view.size();

  std::uint32_t int_value{};
  auto result = std::from_chars(buffer_start, buffer_end, int_value);
  if (result.ec == std::errc::invalid_argument || result.ptr != buffer_end) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidContentLength};
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateHostHeader(const HeaderString& value) {
  //
  // From RFC 7230, https://datatracker.ietf.org/doc/html/rfc7230#section-5.4,
  // and RFC 3986, https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2:
  //
  // Host       = uri-host [ ":" port ]
  // uri-host   = IP-literal / IPv4address / reg-name
  //
  const auto& value_string_view = value.getStringView();

  auto user_info_delimiter = value_string_view.find('@');
  if (user_info_delimiter != absl::string_view::npos) {
    // :authority cannot contain user info, reject the header
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidHost};
  }

  // identify and validate the port, if present
  auto port_delimiter = value_string_view.find(':');
  auto host_string_view = value_string_view.substr(0, port_delimiter);

  if (host_string_view.empty()) {
    // reject empty host, which happens if the authority is just the port (e.g.- ":80").
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidHost};
  }

  if (port_delimiter != absl::string_view::npos) {
    // Validate the port is an integer and a valid port number (uint16_t)
    auto port_string_view = value_string_view.substr(port_delimiter + 1);

    auto port_string_view_size = port_string_view.size();
    if (port_string_view_size == 0 || port_string_view_size > 5) {
      return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidHost};
    }

    auto buffer_start = port_string_view.data();
    auto buffer_end = buffer_start + port_string_view.size();

    std::uint32_t port_integer_value{};
    auto result = std::from_chars(buffer_start, buffer_end, port_integer_value);
    if (result.ec == std::errc::invalid_argument || result.ptr != buffer_end) {
      return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidHost};
    }

    if (port_integer_value == 0 || port_integer_value >= 65535) {
      return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidHost};
    }
  }

  return HeaderEntryValidationResult::success();
}

::Envoy::Http::HeaderValidator::HeaderEntryValidationResult
HeaderValidator::validateGenericPathHeader(const HeaderString& value) {
  const auto& path = value.getStringView();
  auto size = path.size();
  bool is_valid = size > 0;

  for (std::size_t i{0}; i < size && is_valid; ++i) {
    is_valid = testChar(kPathHeaderCharTable, path.at(i));
  }

  if (!is_valid) {
    return {RejectAction::Reject, UhvResponseCodeDetail::get().InvalidUrl};
  }

  return HeaderEntryValidationResult::success();
}

} // namespace EnvoyDefault
} // namespace HeaderValidators
} // namespace Http
} // namespace Extensions
} // namespace Envoy
