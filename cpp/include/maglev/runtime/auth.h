#pragma once

#include <cstdint>
#include <string>

namespace maglev {

class SigningIdentity {
  public:
    static SigningIdentity load(const std::string& private_key_path);
    const std::string& key_id() const;
    const std::string& public_key_payload() const;
    const std::string& private_key_path() const;

  private:
    std::string key_id_;
    std::string public_key_payload_;
    std::string private_key_path_;
};

class RequestSigner {
  public:
    explicit RequestSigner(SigningIdentity identity);

    const SigningIdentity& identity() const;
    std::string build_canonical_request(
        const std::string& method,
        const std::string& pathname,
        std::uint64_t timestamp,
        const std::string& nonce,
        const std::string& body) const;
    std::string sign(const std::string& canonical_request) const;

  private:
    std::string openssl_command_;
    SigningIdentity identity_;
};

}  // namespace maglev
