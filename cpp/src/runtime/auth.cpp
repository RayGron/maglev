#include "maglev/runtime/auth.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "maglev/runtime/config.h"
#include "maglev/runtime/util.h"

namespace maglev {

namespace {

std::string base64_to_base64url_no_pad(std::string value) {
    for (char& ch : value) {
        if (ch == '+') {
            ch = '-';
        } else if (ch == '/') {
            ch = '_';
        }
    }
    while (!value.empty() && value.back() == '=') {
        value.pop_back();
    }
    return value;
}

std::string resolve_openssl_command() {
    const auto env_override = getenv_or_empty("AI_CVSC_OPENSSL_PATH");
    if (!trim(env_override).empty()) {
        return quote_shell_argument(env_override);
    }

#ifdef _WIN32
    if (const auto openssl = find_program_on_path("openssl"); openssl.has_value()) {
        return quote_shell_argument(openssl->string());
    }
#endif

    return "openssl";
}

std::string openssl_capture(const std::string& command) {
    const auto result = run_script(command, std::filesystem::current_path());
    if (result.exit_code != 0) {
        throw std::runtime_error("openssl command failed: " + trim(result.stdout_text + "\n" + result.stderr_text));
    }
    return trim(result.stdout_text);
}

}  // namespace

SigningIdentity SigningIdentity::load(const std::string& private_key_path) {
    const auto openssl = resolve_openssl_command();
    const auto quoted_key = quote_shell_argument(private_key_path);

    const auto public_key_payload =
        openssl_capture(openssl + " pkey -in " + quoted_key + " -pubout -outform DER | " + openssl + " base64 -A");
    const auto fingerprint_base64 = openssl_capture(
        openssl + " pkey -in " + quoted_key + " -pubout -outform DER | " + openssl + " dgst -sha256 -binary | " + openssl +
        " base64 -A");

    SigningIdentity identity;
    identity.key_id_ = "SHA256:" + base64_to_base64url_no_pad(fingerprint_base64);
    identity.public_key_payload_ = public_key_payload;
    identity.private_key_path_ = private_key_path;
    return identity;
}

const std::string& SigningIdentity::key_id() const {
    return key_id_;
}

const std::string& SigningIdentity::public_key_payload() const {
    return public_key_payload_;
}

const std::string& SigningIdentity::private_key_path() const {
    return private_key_path_;
}

RequestSigner::RequestSigner(SigningIdentity identity)
    : openssl_command_(resolve_openssl_command()), identity_(std::move(identity)) {}

const SigningIdentity& RequestSigner::identity() const {
    return identity_;
}

std::string RequestSigner::build_canonical_request(
    const std::string& method,
    const std::string& pathname,
    std::uint64_t timestamp,
    const std::string& nonce,
    const std::string& body) const {
    std::ostringstream stream;
    stream << method << '\n' << pathname << '\n' << timestamp << '\n' << nonce << '\n' << identity_.key_id() << '\n' << body;
    return stream.str();
}

std::string RequestSigner::sign(const std::string& canonical_request) const {
    const auto temp_dir = std::filesystem::temp_directory_path();
    const auto message_path = temp_dir / ("maglev-sign-" + random_hex(8) + ".txt");
    std::string error;
    if (!write_text_file(message_path, canonical_request, error)) {
        throw std::runtime_error(error);
    }

    const auto cleanup = [&]() {
        std::error_code ec;
        std::filesystem::remove(message_path, ec);
    };

    try {
        const auto signature = openssl_capture(
            openssl_command_ + " pkeyutl -sign -inkey " + quote_shell_argument(identity_.private_key_path()) + " -rawin -in " +
            quote_shell_argument(message_path.string()) + " | " + openssl_command_ + " base64 -A");
        cleanup();
        return signature;
    } catch (...) {
        cleanup();
        throw;
    }
}

}  // namespace maglev
