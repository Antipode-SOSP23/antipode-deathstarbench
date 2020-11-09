/**
 * Autogenerated by Thrift Compiler (0.12.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef ComposePostService_H
#define ComposePostService_H

#include <thrift/TDispatchProcessor.h>
#include <thrift/async/TConcurrentClientSyncInfo.h>
#include "social_network_types.h"

namespace social_network {

#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning (disable : 4250 ) //inheriting methods via dominance
#endif

class ComposePostServiceIf {
 public:
  virtual ~ComposePostServiceIf() {}
  virtual void UploadText(BaseRpcResponse& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) = 0;
  virtual void UploadMedia(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier) = 0;
  virtual void UploadUniqueId(BaseRpcResponse& _return, const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier) = 0;
  virtual void UploadCreator(BaseRpcResponse& _return, const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier) = 0;
  virtual void UploadUrls(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier) = 0;
  virtual void UploadUserMentions(BaseRpcResponse& _return, const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier) = 0;
};

class ComposePostServiceIfFactory {
 public:
  typedef ComposePostServiceIf Handler;

  virtual ~ComposePostServiceIfFactory() {}

  virtual ComposePostServiceIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(ComposePostServiceIf* /* handler */) = 0;
};

class ComposePostServiceIfSingletonFactory : virtual public ComposePostServiceIfFactory {
 public:
  ComposePostServiceIfSingletonFactory(const ::apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf>& iface) : iface_(iface) {}
  virtual ~ComposePostServiceIfSingletonFactory() {}

  virtual ComposePostServiceIf* getHandler(const ::apache::thrift::TConnectionInfo&) {
    return iface_.get();
  }
  virtual void releaseHandler(ComposePostServiceIf* /* handler */) {}

 protected:
  ::apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> iface_;
};

class ComposePostServiceNull : virtual public ComposePostServiceIf {
 public:
  virtual ~ComposePostServiceNull() {}
  void UploadText(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const std::string& /* text */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
  void UploadMedia(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const std::vector<Media> & /* media */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
  void UploadUniqueId(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const int64_t /* post_id */, const PostType::type /* post_type */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
  void UploadCreator(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const Creator& /* creator */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
  void UploadUrls(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const std::vector<Url> & /* urls */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
  void UploadUserMentions(BaseRpcResponse& /* _return */, const int64_t /* req_id */, const std::vector<UserMention> & /* user_mentions */, const std::map<std::string, std::string> & /* carrier */) {
    return;
  }
};

typedef struct _ComposePostService_UploadText_args__isset {
  _ComposePostService_UploadText_args__isset() : req_id(false), text(false), carrier(false) {}
  bool req_id :1;
  bool text :1;
  bool carrier :1;
} _ComposePostService_UploadText_args__isset;

class ComposePostService_UploadText_args {
 public:

  ComposePostService_UploadText_args(const ComposePostService_UploadText_args&);
  ComposePostService_UploadText_args& operator=(const ComposePostService_UploadText_args&);
  ComposePostService_UploadText_args() : req_id(0), text() {
  }

  virtual ~ComposePostService_UploadText_args() throw();
  int64_t req_id;
  std::string text;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadText_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_text(const std::string& val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadText_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(text == rhs.text))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadText_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadText_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadText_pargs {
 public:


  virtual ~ComposePostService_UploadText_pargs() throw();
  const int64_t* req_id;
  const std::string* text;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadText_result__isset {
  _ComposePostService_UploadText_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadText_result__isset;

class ComposePostService_UploadText_result {
 public:

  ComposePostService_UploadText_result(const ComposePostService_UploadText_result&);
  ComposePostService_UploadText_result& operator=(const ComposePostService_UploadText_result&);
  ComposePostService_UploadText_result() {
  }

  virtual ~ComposePostService_UploadText_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadText_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadText_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadText_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadText_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadText_presult__isset {
  _ComposePostService_UploadText_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadText_presult__isset;

class ComposePostService_UploadText_presult {
 public:


  virtual ~ComposePostService_UploadText_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadText_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _ComposePostService_UploadMedia_args__isset {
  _ComposePostService_UploadMedia_args__isset() : req_id(false), media(false), carrier(false) {}
  bool req_id :1;
  bool media :1;
  bool carrier :1;
} _ComposePostService_UploadMedia_args__isset;

class ComposePostService_UploadMedia_args {
 public:

  ComposePostService_UploadMedia_args(const ComposePostService_UploadMedia_args&);
  ComposePostService_UploadMedia_args& operator=(const ComposePostService_UploadMedia_args&);
  ComposePostService_UploadMedia_args() : req_id(0) {
  }

  virtual ~ComposePostService_UploadMedia_args() throw();
  int64_t req_id;
  std::vector<Media>  media;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadMedia_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_media(const std::vector<Media> & val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadMedia_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(media == rhs.media))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadMedia_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadMedia_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadMedia_pargs {
 public:


  virtual ~ComposePostService_UploadMedia_pargs() throw();
  const int64_t* req_id;
  const std::vector<Media> * media;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadMedia_result__isset {
  _ComposePostService_UploadMedia_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadMedia_result__isset;

class ComposePostService_UploadMedia_result {
 public:

  ComposePostService_UploadMedia_result(const ComposePostService_UploadMedia_result&);
  ComposePostService_UploadMedia_result& operator=(const ComposePostService_UploadMedia_result&);
  ComposePostService_UploadMedia_result() {
  }

  virtual ~ComposePostService_UploadMedia_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadMedia_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadMedia_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadMedia_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadMedia_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadMedia_presult__isset {
  _ComposePostService_UploadMedia_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadMedia_presult__isset;

class ComposePostService_UploadMedia_presult {
 public:


  virtual ~ComposePostService_UploadMedia_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadMedia_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _ComposePostService_UploadUniqueId_args__isset {
  _ComposePostService_UploadUniqueId_args__isset() : req_id(false), post_id(false), post_type(false), carrier(false) {}
  bool req_id :1;
  bool post_id :1;
  bool post_type :1;
  bool carrier :1;
} _ComposePostService_UploadUniqueId_args__isset;

class ComposePostService_UploadUniqueId_args {
 public:

  ComposePostService_UploadUniqueId_args(const ComposePostService_UploadUniqueId_args&);
  ComposePostService_UploadUniqueId_args& operator=(const ComposePostService_UploadUniqueId_args&);
  ComposePostService_UploadUniqueId_args() : req_id(0), post_id(0), post_type((PostType::type)0) {
  }

  virtual ~ComposePostService_UploadUniqueId_args() throw();
  int64_t req_id;
  int64_t post_id;
  PostType::type post_type;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadUniqueId_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_post_id(const int64_t val);

  void __set_post_type(const PostType::type val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadUniqueId_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(post_id == rhs.post_id))
      return false;
    if (!(post_type == rhs.post_type))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUniqueId_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUniqueId_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadUniqueId_pargs {
 public:


  virtual ~ComposePostService_UploadUniqueId_pargs() throw();
  const int64_t* req_id;
  const int64_t* post_id;
  const PostType::type* post_type;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUniqueId_result__isset {
  _ComposePostService_UploadUniqueId_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUniqueId_result__isset;

class ComposePostService_UploadUniqueId_result {
 public:

  ComposePostService_UploadUniqueId_result(const ComposePostService_UploadUniqueId_result&);
  ComposePostService_UploadUniqueId_result& operator=(const ComposePostService_UploadUniqueId_result&);
  ComposePostService_UploadUniqueId_result() {
  }

  virtual ~ComposePostService_UploadUniqueId_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadUniqueId_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadUniqueId_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUniqueId_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUniqueId_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUniqueId_presult__isset {
  _ComposePostService_UploadUniqueId_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUniqueId_presult__isset;

class ComposePostService_UploadUniqueId_presult {
 public:


  virtual ~ComposePostService_UploadUniqueId_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadUniqueId_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _ComposePostService_UploadCreator_args__isset {
  _ComposePostService_UploadCreator_args__isset() : req_id(false), creator(false), carrier(false) {}
  bool req_id :1;
  bool creator :1;
  bool carrier :1;
} _ComposePostService_UploadCreator_args__isset;

class ComposePostService_UploadCreator_args {
 public:

  ComposePostService_UploadCreator_args(const ComposePostService_UploadCreator_args&);
  ComposePostService_UploadCreator_args& operator=(const ComposePostService_UploadCreator_args&);
  ComposePostService_UploadCreator_args() : req_id(0) {
  }

  virtual ~ComposePostService_UploadCreator_args() throw();
  int64_t req_id;
  Creator creator;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadCreator_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_creator(const Creator& val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadCreator_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(creator == rhs.creator))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadCreator_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadCreator_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadCreator_pargs {
 public:


  virtual ~ComposePostService_UploadCreator_pargs() throw();
  const int64_t* req_id;
  const Creator* creator;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadCreator_result__isset {
  _ComposePostService_UploadCreator_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadCreator_result__isset;

class ComposePostService_UploadCreator_result {
 public:

  ComposePostService_UploadCreator_result(const ComposePostService_UploadCreator_result&);
  ComposePostService_UploadCreator_result& operator=(const ComposePostService_UploadCreator_result&);
  ComposePostService_UploadCreator_result() {
  }

  virtual ~ComposePostService_UploadCreator_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadCreator_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadCreator_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadCreator_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadCreator_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadCreator_presult__isset {
  _ComposePostService_UploadCreator_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadCreator_presult__isset;

class ComposePostService_UploadCreator_presult {
 public:


  virtual ~ComposePostService_UploadCreator_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadCreator_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _ComposePostService_UploadUrls_args__isset {
  _ComposePostService_UploadUrls_args__isset() : req_id(false), urls(false), carrier(false) {}
  bool req_id :1;
  bool urls :1;
  bool carrier :1;
} _ComposePostService_UploadUrls_args__isset;

class ComposePostService_UploadUrls_args {
 public:

  ComposePostService_UploadUrls_args(const ComposePostService_UploadUrls_args&);
  ComposePostService_UploadUrls_args& operator=(const ComposePostService_UploadUrls_args&);
  ComposePostService_UploadUrls_args() : req_id(0) {
  }

  virtual ~ComposePostService_UploadUrls_args() throw();
  int64_t req_id;
  std::vector<Url>  urls;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadUrls_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_urls(const std::vector<Url> & val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadUrls_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(urls == rhs.urls))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUrls_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUrls_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadUrls_pargs {
 public:


  virtual ~ComposePostService_UploadUrls_pargs() throw();
  const int64_t* req_id;
  const std::vector<Url> * urls;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUrls_result__isset {
  _ComposePostService_UploadUrls_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUrls_result__isset;

class ComposePostService_UploadUrls_result {
 public:

  ComposePostService_UploadUrls_result(const ComposePostService_UploadUrls_result&);
  ComposePostService_UploadUrls_result& operator=(const ComposePostService_UploadUrls_result&);
  ComposePostService_UploadUrls_result() {
  }

  virtual ~ComposePostService_UploadUrls_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadUrls_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadUrls_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUrls_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUrls_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUrls_presult__isset {
  _ComposePostService_UploadUrls_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUrls_presult__isset;

class ComposePostService_UploadUrls_presult {
 public:


  virtual ~ComposePostService_UploadUrls_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadUrls_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _ComposePostService_UploadUserMentions_args__isset {
  _ComposePostService_UploadUserMentions_args__isset() : req_id(false), user_mentions(false), carrier(false) {}
  bool req_id :1;
  bool user_mentions :1;
  bool carrier :1;
} _ComposePostService_UploadUserMentions_args__isset;

class ComposePostService_UploadUserMentions_args {
 public:

  ComposePostService_UploadUserMentions_args(const ComposePostService_UploadUserMentions_args&);
  ComposePostService_UploadUserMentions_args& operator=(const ComposePostService_UploadUserMentions_args&);
  ComposePostService_UploadUserMentions_args() : req_id(0) {
  }

  virtual ~ComposePostService_UploadUserMentions_args() throw();
  int64_t req_id;
  std::vector<UserMention>  user_mentions;
  std::map<std::string, std::string>  carrier;

  _ComposePostService_UploadUserMentions_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_user_mentions(const std::vector<UserMention> & val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const ComposePostService_UploadUserMentions_args & rhs) const
  {
    if (!(req_id == rhs.req_id))
      return false;
    if (!(user_mentions == rhs.user_mentions))
      return false;
    if (!(carrier == rhs.carrier))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUserMentions_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUserMentions_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class ComposePostService_UploadUserMentions_pargs {
 public:


  virtual ~ComposePostService_UploadUserMentions_pargs() throw();
  const int64_t* req_id;
  const std::vector<UserMention> * user_mentions;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUserMentions_result__isset {
  _ComposePostService_UploadUserMentions_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUserMentions_result__isset;

class ComposePostService_UploadUserMentions_result {
 public:

  ComposePostService_UploadUserMentions_result(const ComposePostService_UploadUserMentions_result&);
  ComposePostService_UploadUserMentions_result& operator=(const ComposePostService_UploadUserMentions_result&);
  ComposePostService_UploadUserMentions_result() {
  }

  virtual ~ComposePostService_UploadUserMentions_result() throw();
  BaseRpcResponse success;
  ServiceException se;

  _ComposePostService_UploadUserMentions_result__isset __isset;

  void __set_success(const BaseRpcResponse& val);

  void __set_se(const ServiceException& val);

  bool operator == (const ComposePostService_UploadUserMentions_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const ComposePostService_UploadUserMentions_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const ComposePostService_UploadUserMentions_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _ComposePostService_UploadUserMentions_presult__isset {
  _ComposePostService_UploadUserMentions_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _ComposePostService_UploadUserMentions_presult__isset;

class ComposePostService_UploadUserMentions_presult {
 public:


  virtual ~ComposePostService_UploadUserMentions_presult() throw();
  BaseRpcResponse* success;
  ServiceException se;

  _ComposePostService_UploadUserMentions_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class ComposePostServiceClient : virtual public ComposePostServiceIf {
 public:
  ComposePostServiceClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  ComposePostServiceClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void UploadText(BaseRpcResponse& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  void send_UploadText(const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  void recv_UploadText(BaseRpcResponse& _return);
  void UploadMedia(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier);
  void send_UploadMedia(const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier);
  void recv_UploadMedia(BaseRpcResponse& _return);
  void UploadUniqueId(BaseRpcResponse& _return, const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier);
  void send_UploadUniqueId(const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier);
  void recv_UploadUniqueId(BaseRpcResponse& _return);
  void UploadCreator(BaseRpcResponse& _return, const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier);
  void send_UploadCreator(const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier);
  void recv_UploadCreator(BaseRpcResponse& _return);
  void UploadUrls(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier);
  void send_UploadUrls(const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier);
  void recv_UploadUrls(BaseRpcResponse& _return);
  void UploadUserMentions(BaseRpcResponse& _return, const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier);
  void send_UploadUserMentions(const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier);
  void recv_UploadUserMentions(BaseRpcResponse& _return);
 protected:
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
};

class ComposePostServiceProcessor : public ::apache::thrift::TDispatchProcessor {
 protected:
  ::apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> iface_;
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext);
 private:
  typedef  void (ComposePostServiceProcessor::*ProcessFunction)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_UploadText(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_UploadMedia(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_UploadUniqueId(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_UploadCreator(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_UploadUrls(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_UploadUserMentions(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  ComposePostServiceProcessor(::apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> iface) :
    iface_(iface) {
    processMap_["UploadText"] = &ComposePostServiceProcessor::process_UploadText;
    processMap_["UploadMedia"] = &ComposePostServiceProcessor::process_UploadMedia;
    processMap_["UploadUniqueId"] = &ComposePostServiceProcessor::process_UploadUniqueId;
    processMap_["UploadCreator"] = &ComposePostServiceProcessor::process_UploadCreator;
    processMap_["UploadUrls"] = &ComposePostServiceProcessor::process_UploadUrls;
    processMap_["UploadUserMentions"] = &ComposePostServiceProcessor::process_UploadUserMentions;
  }

  virtual ~ComposePostServiceProcessor() {}
};

class ComposePostServiceProcessorFactory : public ::apache::thrift::TProcessorFactory {
 public:
  ComposePostServiceProcessorFactory(const ::apache::thrift::stdcxx::shared_ptr< ComposePostServiceIfFactory >& handlerFactory) :
      handlerFactory_(handlerFactory) {}

  ::apache::thrift::stdcxx::shared_ptr< ::apache::thrift::TProcessor > getProcessor(const ::apache::thrift::TConnectionInfo& connInfo);

 protected:
  ::apache::thrift::stdcxx::shared_ptr< ComposePostServiceIfFactory > handlerFactory_;
};

class ComposePostServiceMultiface : virtual public ComposePostServiceIf {
 public:
  ComposePostServiceMultiface(std::vector<apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~ComposePostServiceMultiface() {}
 protected:
  std::vector<apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> > ifaces_;
  ComposePostServiceMultiface() {}
  void add(::apache::thrift::stdcxx::shared_ptr<ComposePostServiceIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  void UploadText(BaseRpcResponse& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadText(_return, req_id, text, carrier);
    }
    ifaces_[i]->UploadText(_return, req_id, text, carrier);
    return;
  }

  void UploadMedia(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadMedia(_return, req_id, media, carrier);
    }
    ifaces_[i]->UploadMedia(_return, req_id, media, carrier);
    return;
  }

  void UploadUniqueId(BaseRpcResponse& _return, const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadUniqueId(_return, req_id, post_id, post_type, carrier);
    }
    ifaces_[i]->UploadUniqueId(_return, req_id, post_id, post_type, carrier);
    return;
  }

  void UploadCreator(BaseRpcResponse& _return, const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadCreator(_return, req_id, creator, carrier);
    }
    ifaces_[i]->UploadCreator(_return, req_id, creator, carrier);
    return;
  }

  void UploadUrls(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadUrls(_return, req_id, urls, carrier);
    }
    ifaces_[i]->UploadUrls(_return, req_id, urls, carrier);
    return;
  }

  void UploadUserMentions(BaseRpcResponse& _return, const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->UploadUserMentions(_return, req_id, user_mentions, carrier);
    }
    ifaces_[i]->UploadUserMentions(_return, req_id, user_mentions, carrier);
    return;
  }

};

// The 'concurrent' client is a thread safe client that correctly handles
// out of order responses.  It is slower than the regular client, so should
// only be used when you need to share a connection among multiple threads
class ComposePostServiceConcurrentClient : virtual public ComposePostServiceIf {
 public:
  ComposePostServiceConcurrentClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  ComposePostServiceConcurrentClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void UploadText(BaseRpcResponse& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadText(const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  void recv_UploadText(BaseRpcResponse& _return, const int32_t seqid);
  void UploadMedia(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadMedia(const int64_t req_id, const std::vector<Media> & media, const std::map<std::string, std::string> & carrier);
  void recv_UploadMedia(BaseRpcResponse& _return, const int32_t seqid);
  void UploadUniqueId(BaseRpcResponse& _return, const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadUniqueId(const int64_t req_id, const int64_t post_id, const PostType::type post_type, const std::map<std::string, std::string> & carrier);
  void recv_UploadUniqueId(BaseRpcResponse& _return, const int32_t seqid);
  void UploadCreator(BaseRpcResponse& _return, const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadCreator(const int64_t req_id, const Creator& creator, const std::map<std::string, std::string> & carrier);
  void recv_UploadCreator(BaseRpcResponse& _return, const int32_t seqid);
  void UploadUrls(BaseRpcResponse& _return, const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadUrls(const int64_t req_id, const std::vector<Url> & urls, const std::map<std::string, std::string> & carrier);
  void recv_UploadUrls(BaseRpcResponse& _return, const int32_t seqid);
  void UploadUserMentions(BaseRpcResponse& _return, const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier);
  int32_t send_UploadUserMentions(const int64_t req_id, const std::vector<UserMention> & user_mentions, const std::map<std::string, std::string> & carrier);
  void recv_UploadUserMentions(BaseRpcResponse& _return, const int32_t seqid);
 protected:
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
  ::apache::thrift::async::TConcurrentClientSyncInfo sync_;
};

#ifdef _MSC_VER
  #pragma warning( pop )
#endif

} // namespace

#endif
