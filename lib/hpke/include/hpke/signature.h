#pragma once

#include <memory>

#include <bytes/bytes.h>
using namespace bytes_ns;

namespace hpke {

struct Signature
{
  enum struct ID
  {
    P256_SHA256,
    P384_SHA384,
    P521_SHA512,
    Ed25519,
    Ed448,
  };

  template<Signature::ID id>
  static const Signature& get();

  virtual ~Signature() = default;

  struct PublicKey
  {
    virtual ~PublicKey() = default;
  };

  struct PrivateKey
  {
    virtual ~PrivateKey() = default;
    virtual std::unique_ptr<PublicKey> public_key() const = 0;
  };

  virtual std::unique_ptr<PrivateKey> generate_key_pair() const = 0;
  virtual std::unique_ptr<PrivateKey> derive_key_pair(
    const bytes& ikm) const = 0;

  virtual bytes serialize(const PublicKey& pk) const = 0;
  virtual std::unique_ptr<PublicKey> deserialize(const bytes& enc) const = 0;

  virtual bytes serialize_private(const PrivateKey& sk) const;
  virtual std::unique_ptr<PrivateKey> deserialize_private(
    const bytes& skm) const;

  virtual bytes sign(const bytes& data, const PrivateKey& sk) const = 0;
  virtual bool verify(const bytes& data,
                      const bytes& sig,
                      const PublicKey& pk) const = 0;
};

}