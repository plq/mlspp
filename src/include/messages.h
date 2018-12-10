#pragma once

#include "common.h"
#include "crypto.h"
#include "ratchet_tree.h"
#include "roster.h"
#include "tls_syntax.h"

namespace mls {

// struct {
//     CipherSuite cipher_suites<0..255>; // ignored
//     DHPublicKey init_keys<1..2^16-1>;  // only use first
//     SignatureScheme algorithm;
//     SignaturePublicKey identity_key;
//     tls::opaque signature<0..2^16-1>;
// } UserInitKey;
struct UserInitKey
{
  tls::vector<CipherSuite, 1> cipher_suites;
  tls::vector<tls::opaque<2>, 2> init_keys; // Postpone crypto parsing
  SignatureScheme algorithm;
  SignaturePublicKey identity_key;
  tls::opaque<2> signature;

  // XXX(rlb@ipv.sx): This is kind of inelegant, but we need a dummy
  // value here until it gets overwritten by reading from data.  The
  // alternative is to have a default ctor for SignaturePublicKey,
  // which seems worse.
  UserInitKey()
    : identity_key(SignatureScheme::P256_SHA256)
  {}

  void add_init_key(const DHPublicKey& pub);
  void sign(const SignaturePrivateKey& identity_priv);
  bool verify() const;
  bytes to_be_signed() const;
};

bool
operator==(const UserInitKey& lhs, const UserInitKey& rhs);
tls::ostream&
operator<<(tls::ostream& out, const UserInitKey& obj);
tls::istream&
operator>>(tls::istream& in, UserInitKey& obj);

// struct {
//   opaque group_id<0..255>;
//   uint32 epoch;
//   CipherSuite cipher_suite;
//   Credential roster<1..2^32-1>;
//   PublicKey tree<1..2^32-1>;
//   GroupOperation transcript<0..2^32-1>;
//   opaque init_secret<0..255>;
//   opaque leaf_secret<0..255>;
// } Welcome;
struct GroupOperation;
struct Welcome
{
  tls::opaque<1> group_id;
  epoch_t epoch;
  CipherSuite cipher_suite;
  Roster roster;
  RatchetTree tree;
  tls::vector<GroupOperation, 4> transcript;
  tls::opaque<1> init_secret;
  tls::opaque<1> leaf_secret;

  // This ctor should only be used for serialization.  The tree is
  // initialized to a dummy state.
  //
  // XXX: Need to update unmarshal to set the tree properly
  Welcome()
    : tree(CipherSuite::P256_SHA256_AES128GCM)
  {}

  Welcome(tls::opaque<2> group_id,
          epoch_t epoch,
          CipherSuite cipher_suite,
          Roster roster,
          RatchetTree tree,
          tls::vector<GroupOperation, 3> transcript,
          tls::opaque<1> init_secret,
          tls::opaque<1> leaf_secret)
    : group_id(group_id)
    , epoch(epoch)
    , cipher_suite(cipher_suite)
    , roster(roster)
    , tree(tree)
    , transcript(transcript)
    , init_secret(init_secret)
    , leaf_secret(leaf_secret)
  {}
};

bool
operator==(const Welcome& lhs, const Welcome& rhs);
tls::ostream&
operator<<(tls::ostream& out, const Welcome& obj);
tls::istream&
operator>>(tls::istream& in, Welcome& obj);

// enum { ... } GroupOperationType;
enum class GroupOperationType : uint8_t
{
  add = 1,
  update = 2,
  remove = 3,
};

tls::ostream&
operator<<(tls::ostream& out, const GroupOperationType& obj);
tls::istream&
operator>>(tls::istream& in, GroupOperationType& obj);

// struct {
//     DirectPath path<1..2^16-1>;
//     UserInitKey init_key;
// } Add;
struct Add : public CipherAware
{
public:
  RatchetPath path;
  UserInitKey init_key;

  Add(CipherSuite suite)
    : CipherAware(suite)
    , path(suite)
  {}

  Add(const RatchetPath& path, const UserInitKey& init_key)
    : CipherAware(path)
    , path(path)
    , init_key(init_key)
  {}

  static const GroupOperationType type;
};

bool
operator==(const Add& lhs, const Add& rhs);
tls::ostream&
operator<<(tls::ostream& out, const Add& obj);
tls::istream&
operator>>(tls::istream& in, Add& obj);

// struct {
//     DHPublicKey path<1..2^16-1>;
// } Update;
struct Update : public CipherAware
{
public:
  RatchetPath path;

  Update(CipherSuite suite)
    : CipherAware(suite)
    , path(suite)
  {}

  Update(const RatchetPath& path)
    : CipherAware(path)
    , path(path)
  {}

  static const GroupOperationType type;
};

bool
operator==(const Update& lhs, const Update& rhs);
tls::ostream&
operator<<(tls::ostream& out, const Update& obj);
tls::istream&
operator>>(tls::istream& in, Update& obj);

// struct {
//     uint32 removed;
//     DHPublicKey path<1..2^16-1>;
// } Remove;
struct Remove : public CipherAware
{
public:
  uint32_t removed;
  RatchetPath path;

  Remove(CipherSuite suite)
    : CipherAware(suite)
    , path(suite)
  {}

  Remove(uint32_t removed, const RatchetPath& path)
    : CipherAware(path)
    , removed(removed)
    , path(path)
  {}

  static const GroupOperationType type;
};

bool
operator==(const Remove& lhs, const Remove& rhs);
tls::ostream&
operator<<(tls::ostream& out, const Remove& obj);
tls::istream&
operator>>(tls::istream& in, Remove& obj);

// Container class for all operations
//
// struct {
//     GroupOperationType msg_type;
//     select (GroupOperation.msg_type) {
//         case init:      Init;
//         case add:       Add;
//         case update:    Update;
//         case remove:    Remove;
//     };
// } GroupOperation;
//
// NB: This is a "pseudo-union" type, in that only one of the struct
// members will be populated with a non-zero value.  This is a bit
// wasteful of memory, but necessary to avoid the silliness of C++
// union types over structs.
struct GroupOperation : public CipherAware
{
  GroupOperationType type;

  Add add;
  Update update;
  Remove remove;

  // XXX
  GroupOperation()
    : CipherAware(CipherSuite::P256_SHA256_AES128GCM)
    , add(CipherSuite::P256_SHA256_AES128GCM)
    , update(CipherSuite::P256_SHA256_AES128GCM)
    , remove(CipherSuite::P256_SHA256_AES128GCM)
  {}

  GroupOperation(CipherSuite suite)
    : CipherAware(suite)
    , add(suite)
    , update(suite)
    , remove(suite)
  {}

  GroupOperation(const Add& add)
    : CipherAware(add)
    , type(add.type)
    , add(add)
    , update(add.cipher_suite())
    , remove(add.cipher_suite())
  {}

  GroupOperation(const Update& update)
    : CipherAware(update)
    , type(update.type)
    , add(update.cipher_suite())
    , update(update)
    , remove(update.cipher_suite())

  {}

  GroupOperation(const Remove& remove)
    : CipherAware(remove)
    , type(remove.type)
    , add(remove.cipher_suite())
    , update(remove.cipher_suite())
    , remove(remove)
  {}
};

bool
operator==(const GroupOperation& lhs, const GroupOperation& rhs);
tls::ostream&
operator<<(tls::ostream& out, const GroupOperation& obj);
tls::istream&
operator>>(tls::istream& in, GroupOperation& obj);

// struct {
//     uint32 prior_epoch;
//     GroupOperation operation;
//
//     uint32 signer_index;
//     SignatureScheme algorithm;
//     opaque signature<1..2^16-1>;
// } Handshake;
struct Handshake : public CipherAware
{
  epoch_t prior_epoch;
  GroupOperation operation;

  uint32_t signer_index;
  tls::opaque<2> signature;

  epoch_t epoch() const { return prior_epoch + 1; }

  Handshake(CipherSuite suite)
    : CipherAware(suite)
    , operation(suite)
  {}

  Handshake(epoch_t prior_epoch,
            const GroupOperation& operation,
            uint32_t signer_index,
            bytes signature)
    : CipherAware(operation)
    , prior_epoch(prior_epoch)
    , operation(operation)
    , signer_index(signer_index)
    , signature(signature)
  {}
};

bool
operator==(const Handshake& lhs, const Handshake& rhs);
tls::ostream&
operator<<(tls::ostream& out, const Handshake& obj);
tls::istream&
operator>>(tls::istream& in, Handshake& obj);

} // namespace mls