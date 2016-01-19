/*
* Copyright (c) 2015 Cossack Labs Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifdef SECURE_SESSION_HPP
#define SECURE_SESSION_HPP

#include <cstring>
#include <vector>
#include <themis/themis.h>
#include "exception.hpp"

namespace themispp{

    class secure_session_callback_interface{
    public:
	virtual const std::vector<uint8_t> get_pub_key_by_id(const std::vector<uint8_t>& id)=0;
    };


    int get_public_key_for_id_callback(const void *id, size_t id_length, void *key_buffer, size_t key_buffer_length, void *user_data){
	std::vector<uint8_t> pubk=((secure_session_callback_interface*)user_data)->get_pub_key_by_id(std::vector<uint8_t>(static_cast<const uint8_t*>(id), static_cast<const uint8_t*>(id)+id_length));
	if(pubk.size()>key_buffer_length){
	    return THEMIS_BUFFER_TOO_SMALL;
	}
	std::memcpy(key_buffer, &pubk[0], pubk.size());
	return THEMIS_SUCCESS;
    }

    class secure_session{
    public:
	secure_session(const std::string& id, const std::vector<uint8_t>& priv_key, secure_session_callback_interface* callbacks):
	    _session(NULL),
	    _res(0),
	    _res_flag(false){
	    _callback.get_public_key_for_id=themis::get_public_key_for_id_callback;
	    _callback.user_data=callbacks;
	    _session=secure_session_create(id.c_str(), id.length(), &priv_key[0], priv_key.size(), &_callback);
	    if(!_session)
		throw themis::exception("secure_session_create failed");
	}

	virtual ~secure_session(){
	    secure_session_destroy(_session);
	}

	secure_session& unwrap(const std::vector<uint8_t>& data){
	    size_t unwrapped_data_length=0;
	    themis_status_t r=secure_session_unwrap(_session, &data[0],data.size(), NULL, &unwrapped_data_length);
	    if(r==THEMIS_SUCCESS){_res_flag=false;_res.resize(0); return *this;}
	    if(r!=THEMIS_BUFFER_TOO_SMALL)
		throw themis::exception("secure_session_unwrap (length determination) failed");
	    _res.resize(unwrapped_data_length);
	    r=secure_session_unwrap(_session, &data[0],data.size(), &_res[0], &unwrapped_data_length);
	    if(r==THEMIS_SSESSION_SEND_OUTPUT_TO_PEER){_res_flag=true; return *this;}
	    _res_flag=false;
	    if(r!=THEMIS_SUCCESS)
		throw themis::exception("secure_session_unwrap failed");
	    return (*this);
	}

	secure_session& wrap(const std::vector<uint8_t>& data){
	    size_t wrapped_message_length=0;
	    if(secure_session_wrap(_session, &data[0], data.size(), NULL, &wrapped_message_length)!=THEMIS_BUFFER_TOO_SMALL)
		throw themis::exception("secure_session_wrap (length determination) failed");
	    _res.resize(wrapped_message_length);
	    if(secure_session_wrap(_session, &data[0], data.size(), &_res[0], &wrapped_message_length)!=THEMIS_SUCCESS)
		throw themis::exception("secure_session_wrap failed");
	    return (*this);
	}

	secure_session& init(){
	    size_t init_data_length=0;
	    if(secure_session_generate_connect_request(_session, NULL, &init_data_length)!=THEMIS_BUFFER_TOO_SMALL)
		throw themis::exception("secure_session_generate_connect_request (length determination) failed");
	    _res.resize(init_data_length);
	    if(secure_session_generate_connect_request(_session, &_res[0], &init_data_length)!=THEMIS_SUCCESS)
		throw themis::exception("secure_session_generate_connect_request failed");
	    _res_flag=true;
	    return *this;
	}

	const std::vector<uint8_t>& get(){return _res;}
	const bool is_control(){return _res_flag;}
	const bool is_established(){return secure_session_is_established(_session);}
    private:
	secure_session_t* _session;
	secure_session_user_callbacks_t _callback;
	std::vector<uint8_t> _res;
	bool _res_flag;
    };
}// ns themis

#endif
