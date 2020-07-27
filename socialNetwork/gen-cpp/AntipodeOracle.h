/**
 * Autogenerated by Thrift Compiler (0.12.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef AntipodeOracle_H
#define AntipodeOracle_H

#include <thrift/TDispatchProcessor.h>
#include <thrift/async/TConcurrentClientSyncInfo.h>
#include "social_network_types.h"

namespace social_network {

#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning (disable : 4250 ) //inheriting methods via dominance 
#endif

class AntipodeOracleIf {
 public:
  virtual ~AntipodeOracleIf() {}
  virtual bool MakeVisible(const int64_t object_id) = 0;
  virtual bool IsVisible(const int64_t object_id) = 0;
};

class AntipodeOracleIfFactory {
 public:
  typedef AntipodeOracleIf Handler;

  virtual ~AntipodeOracleIfFactory() {}

  virtual AntipodeOracleIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(AntipodeOracleIf* /* handler */) = 0;
};

class AntipodeOracleIfSingletonFactory : virtual public AntipodeOracleIfFactory {
 public:
  AntipodeOracleIfSingletonFactory(const ::apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf>& iface) : iface_(iface) {}
  virtual ~AntipodeOracleIfSingletonFactory() {}

  virtual AntipodeOracleIf* getHandler(const ::apache::thrift::TConnectionInfo&) {
    return iface_.get();
  }
  virtual void releaseHandler(AntipodeOracleIf* /* handler */) {}

 protected:
  ::apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> iface_;
};

class AntipodeOracleNull : virtual public AntipodeOracleIf {
 public:
  virtual ~AntipodeOracleNull() {}
  bool MakeVisible(const int64_t /* object_id */) {
    bool _return = false;
    return _return;
  }
  bool IsVisible(const int64_t /* object_id */) {
    bool _return = false;
    return _return;
  }
};

typedef struct _AntipodeOracle_MakeVisible_args__isset {
  _AntipodeOracle_MakeVisible_args__isset() : object_id(false) {}
  bool object_id :1;
} _AntipodeOracle_MakeVisible_args__isset;

class AntipodeOracle_MakeVisible_args {
 public:

  AntipodeOracle_MakeVisible_args(const AntipodeOracle_MakeVisible_args&);
  AntipodeOracle_MakeVisible_args& operator=(const AntipodeOracle_MakeVisible_args&);
  AntipodeOracle_MakeVisible_args() : object_id(0) {
  }

  virtual ~AntipodeOracle_MakeVisible_args() throw();
  int64_t object_id;

  _AntipodeOracle_MakeVisible_args__isset __isset;

  void __set_object_id(const int64_t val);

  bool operator == (const AntipodeOracle_MakeVisible_args & rhs) const
  {
    if (!(object_id == rhs.object_id))
      return false;
    return true;
  }
  bool operator != (const AntipodeOracle_MakeVisible_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const AntipodeOracle_MakeVisible_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class AntipodeOracle_MakeVisible_pargs {
 public:


  virtual ~AntipodeOracle_MakeVisible_pargs() throw();
  const int64_t* object_id;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _AntipodeOracle_MakeVisible_result__isset {
  _AntipodeOracle_MakeVisible_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _AntipodeOracle_MakeVisible_result__isset;

class AntipodeOracle_MakeVisible_result {
 public:

  AntipodeOracle_MakeVisible_result(const AntipodeOracle_MakeVisible_result&);
  AntipodeOracle_MakeVisible_result& operator=(const AntipodeOracle_MakeVisible_result&);
  AntipodeOracle_MakeVisible_result() : success(0) {
  }

  virtual ~AntipodeOracle_MakeVisible_result() throw();
  bool success;
  ServiceException se;

  _AntipodeOracle_MakeVisible_result__isset __isset;

  void __set_success(const bool val);

  void __set_se(const ServiceException& val);

  bool operator == (const AntipodeOracle_MakeVisible_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const AntipodeOracle_MakeVisible_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const AntipodeOracle_MakeVisible_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _AntipodeOracle_MakeVisible_presult__isset {
  _AntipodeOracle_MakeVisible_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _AntipodeOracle_MakeVisible_presult__isset;

class AntipodeOracle_MakeVisible_presult {
 public:


  virtual ~AntipodeOracle_MakeVisible_presult() throw();
  bool* success;
  ServiceException se;

  _AntipodeOracle_MakeVisible_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _AntipodeOracle_IsVisible_args__isset {
  _AntipodeOracle_IsVisible_args__isset() : object_id(false) {}
  bool object_id :1;
} _AntipodeOracle_IsVisible_args__isset;

class AntipodeOracle_IsVisible_args {
 public:

  AntipodeOracle_IsVisible_args(const AntipodeOracle_IsVisible_args&);
  AntipodeOracle_IsVisible_args& operator=(const AntipodeOracle_IsVisible_args&);
  AntipodeOracle_IsVisible_args() : object_id(0) {
  }

  virtual ~AntipodeOracle_IsVisible_args() throw();
  int64_t object_id;

  _AntipodeOracle_IsVisible_args__isset __isset;

  void __set_object_id(const int64_t val);

  bool operator == (const AntipodeOracle_IsVisible_args & rhs) const
  {
    if (!(object_id == rhs.object_id))
      return false;
    return true;
  }
  bool operator != (const AntipodeOracle_IsVisible_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const AntipodeOracle_IsVisible_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class AntipodeOracle_IsVisible_pargs {
 public:


  virtual ~AntipodeOracle_IsVisible_pargs() throw();
  const int64_t* object_id;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _AntipodeOracle_IsVisible_result__isset {
  _AntipodeOracle_IsVisible_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _AntipodeOracle_IsVisible_result__isset;

class AntipodeOracle_IsVisible_result {
 public:

  AntipodeOracle_IsVisible_result(const AntipodeOracle_IsVisible_result&);
  AntipodeOracle_IsVisible_result& operator=(const AntipodeOracle_IsVisible_result&);
  AntipodeOracle_IsVisible_result() : success(0) {
  }

  virtual ~AntipodeOracle_IsVisible_result() throw();
  bool success;
  ServiceException se;

  _AntipodeOracle_IsVisible_result__isset __isset;

  void __set_success(const bool val);

  void __set_se(const ServiceException& val);

  bool operator == (const AntipodeOracle_IsVisible_result & rhs) const
  {
    if (!(success == rhs.success))
      return false;
    if (!(se == rhs.se))
      return false;
    return true;
  }
  bool operator != (const AntipodeOracle_IsVisible_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const AntipodeOracle_IsVisible_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _AntipodeOracle_IsVisible_presult__isset {
  _AntipodeOracle_IsVisible_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _AntipodeOracle_IsVisible_presult__isset;

class AntipodeOracle_IsVisible_presult {
 public:


  virtual ~AntipodeOracle_IsVisible_presult() throw();
  bool* success;
  ServiceException se;

  _AntipodeOracle_IsVisible_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class AntipodeOracleClient : virtual public AntipodeOracleIf {
 public:
  AntipodeOracleClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  AntipodeOracleClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
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
  bool MakeVisible(const int64_t object_id);
  void send_MakeVisible(const int64_t object_id);
  bool recv_MakeVisible();
  bool IsVisible(const int64_t object_id);
  void send_IsVisible(const int64_t object_id);
  bool recv_IsVisible();
 protected:
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
};

class AntipodeOracleProcessor : public ::apache::thrift::TDispatchProcessor {
 protected:
  ::apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> iface_;
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext);
 private:
  typedef  void (AntipodeOracleProcessor::*ProcessFunction)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_MakeVisible(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_IsVisible(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  AntipodeOracleProcessor(::apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> iface) :
    iface_(iface) {
    processMap_["MakeVisible"] = &AntipodeOracleProcessor::process_MakeVisible;
    processMap_["IsVisible"] = &AntipodeOracleProcessor::process_IsVisible;
  }

  virtual ~AntipodeOracleProcessor() {}
};

class AntipodeOracleProcessorFactory : public ::apache::thrift::TProcessorFactory {
 public:
  AntipodeOracleProcessorFactory(const ::apache::thrift::stdcxx::shared_ptr< AntipodeOracleIfFactory >& handlerFactory) :
      handlerFactory_(handlerFactory) {}

  ::apache::thrift::stdcxx::shared_ptr< ::apache::thrift::TProcessor > getProcessor(const ::apache::thrift::TConnectionInfo& connInfo);

 protected:
  ::apache::thrift::stdcxx::shared_ptr< AntipodeOracleIfFactory > handlerFactory_;
};

class AntipodeOracleMultiface : virtual public AntipodeOracleIf {
 public:
  AntipodeOracleMultiface(std::vector<apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~AntipodeOracleMultiface() {}
 protected:
  std::vector<apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> > ifaces_;
  AntipodeOracleMultiface() {}
  void add(::apache::thrift::stdcxx::shared_ptr<AntipodeOracleIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  bool MakeVisible(const int64_t object_id) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->MakeVisible(object_id);
    }
    return ifaces_[i]->MakeVisible(object_id);
  }

  bool IsVisible(const int64_t object_id) {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->IsVisible(object_id);
    }
    return ifaces_[i]->IsVisible(object_id);
  }

};

// The 'concurrent' client is a thread safe client that correctly handles
// out of order responses.  It is slower than the regular client, so should
// only be used when you need to share a connection among multiple threads
class AntipodeOracleConcurrentClient : virtual public AntipodeOracleIf {
 public:
  AntipodeOracleConcurrentClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  AntipodeOracleConcurrentClient(apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, apache::thrift::stdcxx::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
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
  bool MakeVisible(const int64_t object_id);
  int32_t send_MakeVisible(const int64_t object_id);
  bool recv_MakeVisible(const int32_t seqid);
  bool IsVisible(const int64_t object_id);
  int32_t send_IsVisible(const int64_t object_id);
  bool recv_IsVisible(const int32_t seqid);
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