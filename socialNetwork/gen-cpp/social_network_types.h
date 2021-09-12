/**
 * Autogenerated by Thrift Compiler (0.12.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef social_network_TYPES_H
#define social_network_TYPES_H

#include <iosfwd>

#include <thrift/Thrift.h>
#include <thrift/TApplicationException.h>
#include <thrift/TBase.h>
#include <thrift/protocol/TProtocol.h>
#include <thrift/transport/TTransport.h>

#include <thrift/stdcxx.h>


namespace social_network {

struct ErrorCode {
  enum type {
    SE_CONNPOOL_TIMEOUT = 0,
    SE_THRIFT_CONN_ERROR = 1,
    SE_UNAUTHORIZED = 2,
    SE_MEMCACHED_ERROR = 3,
    SE_MONGODB_ERROR = 4,
    SE_REDIS_ERROR = 5,
    SE_THRIFT_HANDLER_ERROR = 6,
    SE_RABBITMQ_CONN_ERROR = 7,
    SE_FAKE_ERROR = 8
  };
};

extern const std::map<int, const char*> _ErrorCode_VALUES_TO_NAMES;

std::ostream& operator<<(std::ostream& out, const ErrorCode::type& val);

struct PostType {
  enum type {
    POST = 0,
    REPOST = 1,
    REPLY = 2,
    DM = 3
  };
};

extern const std::map<int, const char*> _PostType_VALUES_TO_NAMES;

std::ostream& operator<<(std::ostream& out, const PostType::type& val);

class User;

class ServiceException;

class Media;

class Url;

class UserMention;

class Creator;

class Post;

class BaseRpcResponse;

class LoginRpcResponse;

class UserIdRpcResponse;

class PostRpcResponse;

class PostListRpcResponse;

class UidListRpcResponse;

class UrlListRpcResponse;

typedef struct _User__isset {
  _User__isset() : user_id(false), first_name(false), last_name(false), username(false), password_hashed(false), salt(false) {}
  bool user_id :1;
  bool first_name :1;
  bool last_name :1;
  bool username :1;
  bool password_hashed :1;
  bool salt :1;
} _User__isset;

class User : public virtual ::apache::thrift::TBase {
 public:

  User(const User&);
  User& operator=(const User&);
  User() : user_id(0), first_name(), last_name(), username(), password_hashed(), salt() {
  }

  virtual ~User() throw();
  int64_t user_id;
  std::string first_name;
  std::string last_name;
  std::string username;
  std::string password_hashed;
  std::string salt;

  _User__isset __isset;

  void __set_user_id(const int64_t val);

  void __set_first_name(const std::string& val);

  void __set_last_name(const std::string& val);

  void __set_username(const std::string& val);

  void __set_password_hashed(const std::string& val);

  void __set_salt(const std::string& val);

  bool operator == (const User & rhs) const
  {
    if (!(user_id == rhs.user_id))
      return false;
    if (!(first_name == rhs.first_name))
      return false;
    if (!(last_name == rhs.last_name))
      return false;
    if (!(username == rhs.username))
      return false;
    if (!(password_hashed == rhs.password_hashed))
      return false;
    if (!(salt == rhs.salt))
      return false;
    return true;
  }
  bool operator != (const User &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const User & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(User &a, User &b);

std::ostream& operator<<(std::ostream& out, const User& obj);

typedef struct _ServiceException__isset {
  _ServiceException__isset() : errorCode(false), message(false) {}
  bool errorCode :1;
  bool message :1;
} _ServiceException__isset;

class ServiceException : public ::apache::thrift::TException {
 public:

  ServiceException(const ServiceException&);
  ServiceException& operator=(const ServiceException&);
  ServiceException() : errorCode((ErrorCode::type)0), message() {
  }

  virtual ~ServiceException() throw();
  ErrorCode::type errorCode;
  std::string message;

  _ServiceException__isset __isset;

  void __set_errorCode(const ErrorCode::type val);

  void __set_message(const std::string& val);

  bool operator == (const ServiceException & rhs) const
  {
    if (!(errorCode == rhs.errorCode))
      return false;
    if (!(message == rhs.message))
      return false;
    return true;
  }
  bool operator != (const ServiceException &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ServiceException & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
  mutable std::string thriftTExceptionMessageHolder_;
  const char* what() const throw();
};

void swap(ServiceException &a, ServiceException &b);

std::ostream& operator<<(std::ostream& out, const ServiceException& obj);

typedef struct _Media__isset {
  _Media__isset() : media_id(false), media_type(false) {}
  bool media_id :1;
  bool media_type :1;
} _Media__isset;

class Media : public virtual ::apache::thrift::TBase {
 public:

  Media(const Media&);
  Media& operator=(const Media&);
  Media() : media_id(0), media_type() {
  }

  virtual ~Media() throw();
  int64_t media_id;
  std::string media_type;

  _Media__isset __isset;

  void __set_media_id(const int64_t val);

  void __set_media_type(const std::string& val);

  bool operator == (const Media & rhs) const
  {
    if (!(media_id == rhs.media_id))
      return false;
    if (!(media_type == rhs.media_type))
      return false;
    return true;
  }
  bool operator != (const Media &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Media & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(Media &a, Media &b);

std::ostream& operator<<(std::ostream& out, const Media& obj);

typedef struct _Url__isset {
  _Url__isset() : shortened_url(false), expanded_url(false) {}
  bool shortened_url :1;
  bool expanded_url :1;
} _Url__isset;

class Url : public virtual ::apache::thrift::TBase {
 public:

  Url(const Url&);
  Url& operator=(const Url&);
  Url() : shortened_url(), expanded_url() {
  }

  virtual ~Url() throw();
  std::string shortened_url;
  std::string expanded_url;

  _Url__isset __isset;

  void __set_shortened_url(const std::string& val);

  void __set_expanded_url(const std::string& val);

  bool operator == (const Url & rhs) const
  {
    if (!(shortened_url == rhs.shortened_url))
      return false;
    if (!(expanded_url == rhs.expanded_url))
      return false;
    return true;
  }
  bool operator != (const Url &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Url & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(Url &a, Url &b);

std::ostream& operator<<(std::ostream& out, const Url& obj);

typedef struct _UserMention__isset {
  _UserMention__isset() : user_id(false), username(false) {}
  bool user_id :1;
  bool username :1;
} _UserMention__isset;

class UserMention : public virtual ::apache::thrift::TBase {
 public:

  UserMention(const UserMention&);
  UserMention& operator=(const UserMention&);
  UserMention() : user_id(0), username() {
  }

  virtual ~UserMention() throw();
  int64_t user_id;
  std::string username;

  _UserMention__isset __isset;

  void __set_user_id(const int64_t val);

  void __set_username(const std::string& val);

  bool operator == (const UserMention & rhs) const
  {
    if (!(user_id == rhs.user_id))
      return false;
    if (!(username == rhs.username))
      return false;
    return true;
  }
  bool operator != (const UserMention &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserMention & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(UserMention &a, UserMention &b);

std::ostream& operator<<(std::ostream& out, const UserMention& obj);

typedef struct _Creator__isset {
  _Creator__isset() : user_id(false), username(false) {}
  bool user_id :1;
  bool username :1;
} _Creator__isset;

class Creator : public virtual ::apache::thrift::TBase {
 public:

  Creator(const Creator&);
  Creator& operator=(const Creator&);
  Creator() : user_id(0), username() {
  }

  virtual ~Creator() throw();
  int64_t user_id;
  std::string username;

  _Creator__isset __isset;

  void __set_user_id(const int64_t val);

  void __set_username(const std::string& val);

  bool operator == (const Creator & rhs) const
  {
    if (!(user_id == rhs.user_id))
      return false;
    if (!(username == rhs.username))
      return false;
    return true;
  }
  bool operator != (const Creator &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Creator & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(Creator &a, Creator &b);

std::ostream& operator<<(std::ostream& out, const Creator& obj);

typedef struct _Post__isset {
  _Post__isset() : post_id(false), creator(false), req_id(false), text(false), user_mentions(false), media(false), urls(false), timestamp(false), post_type(false) {}
  bool post_id :1;
  bool creator :1;
  bool req_id :1;
  bool text :1;
  bool user_mentions :1;
  bool media :1;
  bool urls :1;
  bool timestamp :1;
  bool post_type :1;
} _Post__isset;

class Post : public virtual ::apache::thrift::TBase {
 public:

  Post(const Post&);
  Post& operator=(const Post&);
  Post() : post_id(0), req_id(0), text(), timestamp(0), post_type((PostType::type)0) {
  }

  virtual ~Post() throw();
  int64_t post_id;
  Creator creator;
  int64_t req_id;
  std::string text;
  std::vector<UserMention>  user_mentions;
  std::vector<Media>  media;
  std::vector<Url>  urls;
  int64_t timestamp;
  PostType::type post_type;

  _Post__isset __isset;

  void __set_post_id(const int64_t val);

  void __set_creator(const Creator& val);

  void __set_req_id(const int64_t val);

  void __set_text(const std::string& val);

  void __set_user_mentions(const std::vector<UserMention> & val);

  void __set_media(const std::vector<Media> & val);

  void __set_urls(const std::vector<Url> & val);

  void __set_timestamp(const int64_t val);

  void __set_post_type(const PostType::type val);

  bool operator == (const Post & rhs) const
  {
    if (!(post_id == rhs.post_id))
      return false;
    if (!(creator == rhs.creator))
      return false;
    if (!(req_id == rhs.req_id))
      return false;
    if (!(text == rhs.text))
      return false;
    if (!(user_mentions == rhs.user_mentions))
      return false;
    if (!(media == rhs.media))
      return false;
    if (!(urls == rhs.urls))
      return false;
    if (!(timestamp == rhs.timestamp))
      return false;
    if (!(post_type == rhs.post_type))
      return false;
    return true;
  }
  bool operator != (const Post &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const Post & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(Post &a, Post &b);

std::ostream& operator<<(std::ostream& out, const Post& obj);

typedef struct _BaseRpcResponse__isset {
  _BaseRpcResponse__isset() : baggage(false), cscope_json(false) {}
  bool baggage :1;
  bool cscope_json :1;
} _BaseRpcResponse__isset;

class BaseRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  BaseRpcResponse(const BaseRpcResponse&);
  BaseRpcResponse& operator=(const BaseRpcResponse&);
  BaseRpcResponse() : baggage(), cscope_json() {
  }

  virtual ~BaseRpcResponse() throw();
  std::string baggage;
  std::string cscope_json;

  _BaseRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_cscope_json(const std::string& val);

  bool operator == (const BaseRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(cscope_json == rhs.cscope_json))
      return false;
    return true;
  }
  bool operator != (const BaseRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const BaseRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(BaseRpcResponse &a, BaseRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const BaseRpcResponse& obj);

typedef struct _LoginRpcResponse__isset {
  _LoginRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _LoginRpcResponse__isset;

class LoginRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  LoginRpcResponse(const LoginRpcResponse&);
  LoginRpcResponse& operator=(const LoginRpcResponse&);
  LoginRpcResponse() : baggage(), result() {
  }

  virtual ~LoginRpcResponse() throw();
  std::string baggage;
  std::string result;

  _LoginRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const std::string& val);

  bool operator == (const LoginRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const LoginRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const LoginRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(LoginRpcResponse &a, LoginRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const LoginRpcResponse& obj);

typedef struct _UserIdRpcResponse__isset {
  _UserIdRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _UserIdRpcResponse__isset;

class UserIdRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  UserIdRpcResponse(const UserIdRpcResponse&);
  UserIdRpcResponse& operator=(const UserIdRpcResponse&);
  UserIdRpcResponse() : baggage(), result(0) {
  }

  virtual ~UserIdRpcResponse() throw();
  std::string baggage;
  int64_t result;

  _UserIdRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const int64_t val);

  bool operator == (const UserIdRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const UserIdRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserIdRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(UserIdRpcResponse &a, UserIdRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const UserIdRpcResponse& obj);

typedef struct _PostRpcResponse__isset {
  _PostRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _PostRpcResponse__isset;

class PostRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  PostRpcResponse(const PostRpcResponse&);
  PostRpcResponse& operator=(const PostRpcResponse&);
  PostRpcResponse() : baggage() {
  }

  virtual ~PostRpcResponse() throw();
  std::string baggage;
  Post result;

  _PostRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const Post& val);

  bool operator == (const PostRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const PostRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const PostRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(PostRpcResponse &a, PostRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const PostRpcResponse& obj);

typedef struct _PostListRpcResponse__isset {
  _PostListRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _PostListRpcResponse__isset;

class PostListRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  PostListRpcResponse(const PostListRpcResponse&);
  PostListRpcResponse& operator=(const PostListRpcResponse&);
  PostListRpcResponse() : baggage() {
  }

  virtual ~PostListRpcResponse() throw();
  std::string baggage;
  std::vector<Post>  result;

  _PostListRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const std::vector<Post> & val);

  bool operator == (const PostListRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const PostListRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const PostListRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(PostListRpcResponse &a, PostListRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const PostListRpcResponse& obj);

typedef struct _UidListRpcResponse__isset {
  _UidListRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _UidListRpcResponse__isset;

class UidListRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  UidListRpcResponse(const UidListRpcResponse&);
  UidListRpcResponse& operator=(const UidListRpcResponse&);
  UidListRpcResponse() : baggage() {
  }

  virtual ~UidListRpcResponse() throw();
  std::string baggage;
  std::vector<int64_t>  result;

  _UidListRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const std::vector<int64_t> & val);

  bool operator == (const UidListRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const UidListRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UidListRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(UidListRpcResponse &a, UidListRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const UidListRpcResponse& obj);

typedef struct _UrlListRpcResponse__isset {
  _UrlListRpcResponse__isset() : baggage(false), result(false) {}
  bool baggage :1;
  bool result :1;
} _UrlListRpcResponse__isset;

class UrlListRpcResponse : public virtual ::apache::thrift::TBase {
 public:

  UrlListRpcResponse(const UrlListRpcResponse&);
  UrlListRpcResponse& operator=(const UrlListRpcResponse&);
  UrlListRpcResponse() : baggage() {
  }

  virtual ~UrlListRpcResponse() throw();
  std::string baggage;
  std::vector<std::string>  result;

  _UrlListRpcResponse__isset __isset;

  void __set_baggage(const std::string& val);

  void __set_result(const std::vector<std::string> & val);

  bool operator == (const UrlListRpcResponse & rhs) const
  {
    if (!(baggage == rhs.baggage))
      return false;
    if (!(result == rhs.result))
      return false;
    return true;
  }
  bool operator != (const UrlListRpcResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UrlListRpcResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

  virtual void printTo(std::ostream& out) const;
};

void swap(UrlListRpcResponse &a, UrlListRpcResponse &b);

std::ostream& operator<<(std::ostream& out, const UrlListRpcResponse& obj);

} // namespace

#endif
