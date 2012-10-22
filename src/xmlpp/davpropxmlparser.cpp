#include "davpropxmlparser.hpp"

#include <datetime/datetime_utils.h>
#include <status/davixstatusrequest.hpp>


const char * prop_pattern = "prop";
const char * propstat_pattern = "propstat" ;
const char * response_pattern = "response" ;
const char * getlastmodified_pattern = "getlastmodified" ;
const char * creationdate_pattern = "creationdate" ;
const char * getcontentlength_pattern = "getcontentlength" ;
const char * mode_pattern = "mode" ;
const char * href_pattern = "href" ;
const char * resource_type_patern = "resourcetype" ;
const char * collection_patern = "collection" ;
const char * status_pattern = "status" ;


const char* parser_elem_list_start[] = { prop_pattern };


#define DAVIX_XML_REPORT_ERROR(X) \
    do{                           \
        X;                        \
        if(err)                   \
            return -1;            \
    }while(0)


namespace Davix {






/**
  check if the current element origin match the pattern
  if it is the case, force the scope_bool -> TRUE, if already to TRUE -> error
  in a case of a change, return true, else false
*/
inline bool add_scope(bool *scope_bool, const char*  origin, const char* pattern,
                      const bool enter_condition,
                      const bool skip_condition, DavixError** err){
    if(enter_condition && !skip_condition && match_element(origin, pattern)){
        if(*scope_bool==true){
            DavixError::setupError(err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, "parsing error in the webdav request result :" + std::string(origin) + "deplucated");
            return false;
        }
        //std::cout << " in scope " << origin << " while parsing " << std::endl;
        *scope_bool = true;
        return true;
    }
    return false;
}


/**
  act like add_scope but in the opposite way
*/
inline bool remove_scope(bool *scope_bool, const char* origin, const char* pattern,
                        const bool enter_condition,
                        const bool skip_condition, DavixError** err){
    if(enter_condition && !skip_condition && match_element(origin, pattern)){
        if(*scope_bool==false){
            DavixError::setupError(err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, "parsing error in the webdav request result :" + std::string(origin) + "not opended before");
            return false;
        }
        //std::cout << " out scope " << origin << " while parsing " << std::endl;
        *scope_bool = false;
        return true;
    }
    return false;
}



DavPropXMLParser::DavPropXMLParser()
{
    prop_section = propname_section = false;
    response_section = lastmod_section = false;
    creatdate_section = contentlength_section= false;
    mode_ext_section = href_section = false;
    resource_type = status_section= false;
}

DavPropXMLParser::~DavPropXMLParser(){

}



int DavPropXMLParser::compute_new_elem(){
    if(prop_section && propname_section && response_section){
        davix_log_debug(" properties detected ");
        _current_props.clear();
        _current_props.filename = last_filename; // setup the current filename
        _current_props.mode = 0777; // default : fake access to everything
    }
    return 0;
}

int DavPropXMLParser::store_new_elem(){
    if(response_section){
        davix_log_debug(" end of properties... ");
        if( _current_props.req_status > 100
            && _current_props.req_status < 400){
            _props.push_back(_current_props);
        }else
           davix_log_debug(" Bad status code ! properties dropped ");
    }
    return 0;
}

int DavPropXMLParser::check_last_modified(const char* name){
    if(response_section && prop_section && propname_section
          && lastmod_section){ // parse rfc1123 date format
        davix_log_debug(" getlastmodified found -> parse it ");
        GError * tmp_err=NULL;
        time_t t = parse_standard_date(name, &tmp_err);
        if(t == -1){
            DavixError::setupError(&err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, "Invalid last modified date format");
            g_clear_error(&tmp_err);
            return -1;
        }
        davix_log_debug(" getlastmodified found -> value %ld ", t);
        _current_props.mtime = t;
    }
    return 0;
}

int DavPropXMLParser::check_creation_date(const char* name){
    if(response_section && prop_section && propname_section
            && creatdate_section){
        davix_log_debug("creationdate found -> parse it");
        GError * tmp_err1=NULL;
        time_t t = parse_standard_date(name, &tmp_err1);
        if(t == -1){
            DavixError::setupError(&err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, "Invalid creation date format");
            g_clear_error(&tmp_err1);
           return -1;
        }
        davix_log_debug(" creationdate found -> value %ld ", t);
        _current_props.ctime = t;
    }
    return 0;
}

int DavPropXMLParser::check_is_directory(const char* name){
    if(response_section && prop_section && propname_section
            && resource_type){
        bool is_dir=false;
        add_scope(&is_dir, name, collection_patern, propname_section && resource_type, false, &err);
        if(err != NULL)
            return -1;
        if(is_dir){
           davix_log_debug(" directory pattern found -> set flag IS_DIR");
           _current_props.mode |=  S_IFDIR;
        }
    }
    return 0;
}

int DavPropXMLParser::check_content_length(const char* name){
    if(response_section && prop_section && propname_section
             && contentlength_section){
        davix_log_debug(" content length found -> parse it");
        const unsigned long mysize = strtoul(name, NULL, 10);
        if(mysize == ULONG_MAX){
            DavixError::setupError(&err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, " Invalid content length value in dav response");
            errno =0;
            return -1;
        }
        davix_log_debug(" content length found -> %ld", mysize);
        _current_props.size = (off_t) mysize;
    }
    return 0;
}

int DavPropXMLParser::check_mode_ext(const char* name){
    if(response_section && prop_section && propname_section &&
            mode_ext_section){
        davix_log_debug(" mode_t extension for LCGDM found -> parse it");
        const unsigned long mymode = strtoul(name, NULL, 8);
        if(mymode == ULONG_MAX){
            DavixError::setupError(&err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, " Invalid mode_t value for the LCGDM extension");
            errno =0;
            return -1;
        }
        davix_log_debug(" mode_t extension found -> 0%o", (mode_t) mymode);
        _current_props.mode = (mode_t) mymode;
    }
    return 0;
}

int DavPropXMLParser::check_href(const char* c_name){
    if(response_section &&
            href_section){
        davix_log_debug(" href/filename found -> parse it");
        size_t s_name = strlen(c_name);
        char buff_name[s_name+1];
        char * p_end= (char*)mempcpy(buff_name, c_name, s_name);
        *p_end = '\0';
         p_end--;

       while(p_end >  buff_name && *p_end == '/' ){
           *p_end = '\0';
            p_end--;
       }
       if( (p_end = strrchr((char*)buff_name, '/')) != NULL){
           last_filename = p_end+1;
       }else{
           last_filename = buff_name;
        }
       davix_log_debug(" href/filename found -> %s ", last_filename.c_str() );
    }
    return 0;
}

int DavPropXMLParser::check_status(const char* name){
    if(response_section &&
            propname_section && status_section ){
        davix_log_debug(" status found -> parse it");
        char * p1, *p2 = (char*) name;
        while(*p2 == ' ')
            ++p2;
        p1 = strchr(p2,' ');
        if(p1){ // find number field
           p1++;
           p2=p1;
           while(*p2 != ' ' && *p2 != '\0')
               ++p2;
           char buff[p2-p1+1];

           *((char*) mempcpy(buff, p1, p2-p1)) = '\0';
           unsigned long res = strtoul(buff, NULL, 10);
           if(res != ULONG_MAX){
              davix_log_debug(" status value : %ld", res);
              _current_props.req_status = res;
              return 0;
           }


        }
        DavixError::setupError(&err, davix_scope_xml_parser(), StatusCode::WebDavPropertiesParsingError, " Invalid dav status field value");
        errno =0;
       return -1;
    }
    return 0;
}


int DavPropXMLParser::parserStartElemCb(int parent, const char *nspace, const char *name, const char **atts){
  //  std::cout << " name : " << name << std::endl;
    // compute the current scope
    bool new_prop;
    int ret=-1;
    DAVIX_XML_REPORT_ERROR( add_scope(&propname_section, name, propstat_pattern,  response_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( new_prop = add_scope(&prop_section, name, prop_pattern, response_section && propname_section , false, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&status_section, name, status_pattern, propname_section && response_section, prop_section, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&response_section, name, response_pattern, true, prop_section && propname_section, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&lastmod_section, name, getlastmodified_pattern, propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&creatdate_section, name, creationdate_pattern, propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&contentlength_section, name, getcontentlength_pattern,propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&mode_ext_section, name, mode_pattern,propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&href_section, name, href_pattern, response_section, prop_section, &err) );
    DAVIX_XML_REPORT_ERROR( add_scope(&resource_type, name, resource_type_patern, propname_section, false, &err) );
    // mono-balise check
    if( ( ret = check_is_directory(name)) < 0)
            return ret;
    // collect information
    if(new_prop)
        ret= compute_new_elem();
    return (ret==0)?1:-1;
}

int DavPropXMLParser::parserCdataCb(int state, const char *cdata, size_t len){
  //  std::cout << "chars ..." << std::string(cdata, len)<< std::endl;
    char buff[len+1];
    *((char*) mempcpy(buff, cdata, len)) ='\0';

    DAVIX_XML_REPORT_ERROR( check_last_modified(buff) );
    DAVIX_XML_REPORT_ERROR( check_creation_date(buff) );
    DAVIX_XML_REPORT_ERROR( check_content_length(buff) );
    DAVIX_XML_REPORT_ERROR( check_mode_ext(buff) );
    DAVIX_XML_REPORT_ERROR( check_href(buff) );
    DAVIX_XML_REPORT_ERROR( check_status(buff) );
    return 0;
}

int DavPropXMLParser::parserEndElemCb(int state, const char *nspace, const char *name){
    // std::cout << "end name : " << name << std::endl;
    // compute the current scope
    int ret =0;
    bool end_prop;

    DAVIX_XML_REPORT_ERROR( end_prop=  remove_scope(&propname_section, name, propstat_pattern, response_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&prop_section, name, prop_pattern,  response_section && propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&status_section, name, status_pattern, propname_section && response_section, prop_section, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&response_section, name, response_pattern, true, prop_section && propname_section, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&lastmod_section, name, getlastmodified_pattern, propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&creatdate_section, name, creationdate_pattern, propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&contentlength_section, name, getcontentlength_pattern, propname_section, false, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&mode_ext_section, name, mode_pattern, propname_section, false, &err) ); // lcgdm extension for mode_t support
    DAVIX_XML_REPORT_ERROR( remove_scope(&href_section, name, href_pattern, response_section, prop_section, &err) );
    DAVIX_XML_REPORT_ERROR( remove_scope(&resource_type, name, resource_type_patern, propname_section, false, &err) );

    if(end_prop)
        ret = store_new_elem();
    return ret;
}

} // namespace Davix
