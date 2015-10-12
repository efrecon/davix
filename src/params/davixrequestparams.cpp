/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) CERN 2013  
 * Author: Adrien Devresse <adrien.devresse@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
*/

#include <davix_internal.hpp>

#include <utils/davix_logger_internal.hpp>
#include <params/davixrequestparams.hpp>
#include <libs/time_utils.h>



namespace Davix {



const char * default_agent = "libdavix/" DAVIX_VERSION;
volatile int state_value =0;

inline int get_requeste_uid(){
    state_value +=1;
    return state_value;
}


#define SESSION_FLAG_KEEP_ALIVE 0x01

struct X509Data{
    X509Data() : _pair(static_cast<authCallbackClientCertX509>(NULL),static_cast<void*>(NULL)), _x509_fun(), _cred(){}

    std::pair<authCallbackClientCertX509, void*> _pair;

    authFunctionClientCertX509 _x509_fun;

    X509Credential _cred;

    static X509Data* instance(boost::shared_ptr<X509Data> & cred_ptr){
        if(cred_ptr.get() == NULL){
            cred_ptr.reset(new X509Data());
        }
        return cred_ptr.get();
    }

    static X509Data* reset(boost::shared_ptr<X509Data> & cred_ptr){
        cred_ptr.reset(new X509Data());
        return cred_ptr.get();
    }

    int cred_callback(const SessionInfo & info, X509Credential& cert){
        (void) info;
        cert = _cred;
        return 0;
    }

    int c_callback(const SessionInfo & info, X509Credential& cert){
        DavixError* tmp_err=NULL;
        int ret = -1;
        if(_pair.first){
            ret = _pair.first(_pair.second, info, &cert, &tmp_err);
        }
        Davix::checkDavixError(&tmp_err);
        return ret;
    }
};

struct RequestParamsInternal{
    RequestParamsInternal() :
        _ssl_check(true),
        _redirection(true),
        _recursive_mode(false),
        _s3_listing_mode(S3ListingMode::Hierarchical),
        _s3_max_key_entries(10000),
        _ca_path(),
        _x509_data(),
        _idlogpass(),
        _call_loginpswwd(NULL),
        _call_loginpswwd_userdata(NULL),
        _aws_cred(),
        _aws_region(),
        _aws_v2_alternate(false),
        ops_timeout(),
        connexion_timeout(),
        agent_string(default_agent),
        _proto(RequestProtocol::Auto),
        _metalink_mode(MetalinkMode::Auto),
        _customhdr(),
        _proxy_server(),
        _session_flag(SESSION_FLAG_KEEP_ALIVE),
        _state_uid(get_requeste_uid()),
        _transferCb(),
        retry_number(default_retry_number),
        retry_delay(),
        _copy_mode(CopyMode::Push)
    {
        timespec_clear(&connexion_timeout);
        timespec_clear(&ops_timeout);
        connexion_timeout.tv_sec = DAVIX_DEFAULT_CONN_TIMEOUT;
        ops_timeout.tv_sec = DAVIX_DEFAULT_OPS_TIMEOUT;
    }

    virtual ~RequestParamsInternal(){
        if(_idlogpass.second.empty() == false){
            // eradicate password from memory
            for(std::string::iterator it = _idlogpass.second.begin(); it < _idlogpass.second.end(); ++it)
                *it = 'a';
        }

    }
    RequestParamsInternal(const RequestParamsInternal & param_private):
        _ssl_check(param_private._ssl_check),
        _redirection(param_private._redirection),
        _recursive_mode(param_private._recursive_mode),
        _s3_listing_mode(param_private._s3_listing_mode),
        _s3_max_key_entries(param_private._s3_max_key_entries),
        _ca_path(param_private._ca_path),
        _x509_data(param_private._x509_data),
        _idlogpass(param_private._idlogpass),
        _call_loginpswwd(param_private._call_loginpswwd),
        _call_loginpswwd_userdata(param_private._call_loginpswwd_userdata),
        _aws_cred(param_private._aws_cred),
        _aws_region(param_private._aws_region),
        _aws_v2_alternate(param_private._aws_v2_alternate),
        ops_timeout(),
        connexion_timeout(),
        agent_string(param_private.agent_string),
        _proto(param_private._proto),
        _metalink_mode(param_private._metalink_mode),
        _customhdr(param_private._customhdr),
        _proxy_server(param_private._proxy_server),
        _session_flag(param_private._session_flag),
        _state_uid(param_private._state_uid),
        _transferCb(param_private._transferCb),
        retry_number(param_private.retry_number),
        retry_delay(param_private.retry_delay),
        _copy_mode(param_private._copy_mode){

        timespec_copy(&(connexion_timeout), &(param_private.connexion_timeout));
        timespec_copy(&(ops_timeout), &(param_private.ops_timeout));
    }
    bool _ssl_check; // ssl CA check
    bool _redirection; // redirection support
    bool _recursive_mode; // recursive mode for get/put collections

    // s3 bucket listing mode
    S3ListingMode::S3ListingMode _s3_listing_mode;

    // Max number of keys returned by a S3 list bucket request
    unsigned long _s3_max_key_entries; 

    // CA management
    std::vector<std::string> _ca_path;

    // auth info
    boost::shared_ptr<X509Data> _x509_data;

    std::pair<std::string,std::string> _idlogpass;
    authCallbackLoginPasswordBasic _call_loginpswwd;
    void* _call_loginpswwd_userdata;
    std::pair<AwsSecretKey, AwsAccessKey> _aws_cred;
    AwsRegion _aws_region;
    bool _aws_v2_alternate;

    // timeout management
    struct timespec ops_timeout;
    struct timespec connexion_timeout;

    // user agent
    std::string agent_string;

    // proto
    RequestProtocol::Protocol  _proto;
    MetalinkMode::MetalinkMode _metalink_mode;

    // additional custom header lines
    HeaderVec _customhdr;

    // Proxy server URI
    boost::shared_ptr<Uri> _proxy_server;

    // session flag
    int _session_flag;

    // ssl state value, a state uid is used to check if two copy of a requestParam struct are equal
    int _state_uid;

    // transfer cb
    TransferMonitorCB _transferCb;

    // retry attempts
    int retry_number;

    // delay in seconds between retry attempts
    int retry_delay;

    // 3rd party copy mode
    CopyMode::CopyMode _copy_mode;
    
    // method
    inline void regenerateStateUid(){
        _state_uid = get_requeste_uid();
    }

private:
    RequestParamsInternal & operator=(const RequestParamsInternal & params);
};


RequestParams::RequestParams() :
    d_ptr(new RequestParamsInternal())
{

}

RequestParams::RequestParams(const RequestParams& params) :
    d_ptr(new RequestParamsInternal(*(params.d_ptr))){

}




RequestParams::~RequestParams(){
   delete d_ptr;
}

RequestParams::RequestParams(const RequestParams* params) :
    d_ptr( ((params)?(new RequestParamsInternal(*(params->d_ptr))):(new RequestParamsInternal())) ){

}


RequestParams & RequestParams::operator=(const RequestParams & orig){
    if(d_ptr != orig.d_ptr)
        delete d_ptr;
    d_ptr = new RequestParamsInternal(*(orig.d_ptr));
    return *this;
}


bool RequestParams::getSSLCACheck() const{
    return d_ptr->_ssl_check;
}

void RequestParams::setSSLCAcheck(bool chk){
    d_ptr->regenerateStateUid();
    d_ptr->_ssl_check = chk;
}


void RequestParams::setClientCertX509(const X509Credential & cli_cert){
    using namespace std;
    d_ptr->regenerateStateUid();
    X509Data* x509 = X509Data::reset(d_ptr->_x509_data);
    x509->_cred = cli_cert;
    x509->_x509_fun = bind(&X509Data::cred_callback, x509, _1, _2);
}

void RequestParams::setClientLoginPassword(const std::string & login, const std::string & password){
    d_ptr->regenerateStateUid();
    d_ptr->_idlogpass = std::make_pair(login, password);
}

const std::pair<std::string, std::string> & RequestParams::getClientLoginPassword() const{
    return d_ptr->_idlogpass;
}

const X509Credential & RequestParams::getClientCertX509() const{
    X509Data* x509 = X509Data::instance(d_ptr->_x509_data);
    return x509->_cred;
}

/// set a callback for X509 client side dynamic authentication
/// this function overwrite \ref setClientCertX509
void RequestParams::setClientCertCallbackX509(authCallbackClientCertX509 callback, void* userdata){
    using namespace std;
    d_ptr->regenerateStateUid();
    X509Data* x509 = X509Data::reset(d_ptr->_x509_data);
    x509->_pair = std::make_pair(callback, userdata);
    x509->_x509_fun = bind(&X509Data::c_callback, x509, _1, _2);
}


void RequestParams::setClientCertFunctionX509(const authFunctionClientCertX509 &callback){
    d_ptr->regenerateStateUid();
    X509Data* x509 = X509Data::reset(d_ptr->_x509_data);
    x509->_x509_fun= callback;
}

const authFunctionClientCertX509 & RequestParams::getClientCertFunctionX509() const{
    X509Data* x509 = X509Data::instance(d_ptr->_x509_data);
    return x509->_x509_fun;
}

/// return the current client side callback for authentification with the current user data
std::pair<authCallbackClientCertX509,void*> RequestParams::getClientCertCallbackX509() const{
    X509Data* x509 = X509Data::instance(d_ptr->_x509_data);
    return x509->_pair;
}

/// set a callback for X509 client side dynamic authentication
/// this function overwrite \ref setClientCertX509
void RequestParams::setClientLoginPasswordCallback(authCallbackLoginPasswordBasic callback, void* userdata){
    d_ptr->regenerateStateUid();
    d_ptr->_call_loginpswwd = callback;
    d_ptr->_call_loginpswwd_userdata = userdata;
}

/// return the current client side callback for authentification with the current user data
std::pair<authCallbackLoginPasswordBasic,void*> RequestParams::getClientLoginPasswordCallback() const{
    return std::pair<authCallbackLoginPasswordBasic,void*>(d_ptr->_call_loginpswwd, d_ptr->_call_loginpswwd_userdata);
}


void RequestParams::setAwsAuthorizationKeys(const std::string &secret_key, const std::string &access_key){
    d_ptr->_aws_cred = std::pair<AwsSecretKey, AwsAccessKey>(secret_key,access_key);
}

const std::pair<AwsSecretKey, AwsAccessKey> & RequestParams::getAwsAutorizationKeys() const{
    return d_ptr->_aws_cred;
}

void RequestParams::setAwsRegion(const AwsRegion &region) {
    d_ptr->_aws_region = region;
}

const AwsRegion & RequestParams::getAwsRegion() const {
    return d_ptr->_aws_region;
}

void RequestParams::setAwsv2Alternate(const bool &alternate) {
    d_ptr->_aws_v2_alternate = alternate;
}

const bool & RequestParams::getAwsv2Alternate() const {
    return d_ptr->_aws_v2_alternate;
}

void RequestParams::setS3ListingMode(const S3ListingMode::S3ListingMode s3_listing_mode){
    d_ptr->_s3_listing_mode = s3_listing_mode;
}

S3ListingMode::S3ListingMode RequestParams::getS3ListingMode() const{
    return d_ptr->_s3_listing_mode;
}

void RequestParams::setS3MaxKey(const unsigned long s3_max_key_entries){
    d_ptr->_s3_max_key_entries = s3_max_key_entries;
}

unsigned long RequestParams::getS3MaxKey() const{
    return d_ptr->_s3_max_key_entries;    
}

void RequestParams::addCertificateAuthorityPath(const std::string &path){
    d_ptr->regenerateStateUid();
    d_ptr->_ca_path.push_back(path);
}

const std::vector<std::string> & RequestParams::listCertificateAuthorityPath() const{
    return d_ptr->_ca_path;
}


void RequestParams::setConnectionTimeout(struct timespec *conn_timeout1){
    timespec_copy(&(d_ptr->connexion_timeout),conn_timeout1);
}

void RequestParams::setOperationTimeout(struct timespec *ops_timeout1){
    timespec_copy(&(d_ptr->ops_timeout), ops_timeout1);
}

const struct timespec* RequestParams::getConnectionTimeout() const {
    return &d_ptr->connexion_timeout;
}

const struct timespec* RequestParams::getOperationTimeout() const {
    return &d_ptr->ops_timeout;
}

void RequestParams::setTransparentRedirectionSupport(bool redirection){
    d_ptr->regenerateStateUid();
    d_ptr->_redirection = redirection;
}


bool RequestParams::getTransparentRedirectionSupport() const{
    return d_ptr->_redirection;
}

void RequestParams::setOperationRetry(int number_retry){
    d_ptr->retry_number = number_retry;
}

int RequestParams::getOperationRetry() const{
    return d_ptr->retry_number;
}

void RequestParams::setOperationRetryDelay(int delay_retry){
    d_ptr->retry_delay = delay_retry;
}

int RequestParams::getOperationRetryDelay() const{
    return d_ptr->retry_delay;
}

void RequestParams::setTransfertMonitorCb(const TransferMonitorCB &cb){
    d_ptr->_transferCb = cb;
}

const TransferMonitorCB & RequestParams::getTransferMonitorCb() const{
    return d_ptr->_transferCb;
}

const std::string & RequestParams::getUserAgent() const{
    return d_ptr->agent_string;
}

void RequestParams::setUserAgent(const std::string &user_agent){
    d_ptr->regenerateStateUid();
    d_ptr->agent_string = user_agent;
}


RequestProtocol::Protocol RequestParams::getProtocol() const {
    return d_ptr->_proto;
}

void RequestParams::setProtocol(const RequestProtocol::Protocol proto){
    d_ptr->_proto = proto;
}

MetalinkMode::MetalinkMode RequestParams::getMetalinkMode() const{
    return d_ptr->_metalink_mode;
}

void RequestParams::setMetalinkMode(const MetalinkMode::MetalinkMode mode){
    d_ptr->_metalink_mode = mode;
}


void RequestParams::setKeepAlive(const bool keep_alive_flag){
    d_ptr->regenerateStateUid();
    if(keep_alive_flag)
        d_ptr->_session_flag |= SESSION_FLAG_KEEP_ALIVE;
    else
        d_ptr->_session_flag &= ~(SESSION_FLAG_KEEP_ALIVE);
}


bool RequestParams::getKeepAlive() const{
    return d_ptr->_session_flag & SESSION_FLAG_KEEP_ALIVE;
}



void RequestParams::addHeader(const std::string &key, const std::string &val) {

  d_ptr->_customhdr.push_back( std::pair<std::string,std::string>(key, val) );
}

const HeaderVec & RequestParams::getHeaders() const{
  return d_ptr->_customhdr;
}

void RequestParams::setProxyServer(const Uri &proxy_url){
    d_ptr->_proxy_server.reset(new Uri(proxy_url));
}

const Uri* RequestParams::getProxyServer() const{
    return d_ptr->_proxy_server.get();
}


void RequestParams::setCopyMode(const CopyMode::CopyMode copy_mode){
    d_ptr->_copy_mode = copy_mode;
}

CopyMode::CopyMode RequestParams::getCopyMode() const{
    return d_ptr->_copy_mode;
}

void RequestParams::setRecursiveMode(const bool recursive_mode){
    d_ptr->_recursive_mode = recursive_mode;
}

bool RequestParams::getRecursiveMode() const{
    return d_ptr->_recursive_mode;
}

// suppress useless warning
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void* RequestParams::getParmState() const{

    return (void*) (d_ptr->_state_uid);
}

void RequestParams::swap(RequestParams & p){
    std::swap(d_ptr, p.d_ptr);
}


} // namespace Davix


