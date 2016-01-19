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

#include <vector>
#include <themis/themis.h>
#include "exception.hpp"

namespace themispp{

    enum secure_cell_mode{
	SEAL,
	TOKEN_PROTECT,
	CONTEXT_IMPRINT
    };

    class secure_cell{
	public:
	    secure_cell(const std::vector<uint8_t>& password):
		_password(password),
		_res(0),
		_token(0){}

	    virtual secure_cell& encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context)=0;
	    virtual secure_cell& decrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context)=0;

	    std::vector<uint8_t>& get(){
		return _res;
	    }
	protected:
	    std::vector<uint8_t> _password;
	    std::vector<uint8_t> _res;
	    std::vector<uint8_t> _token;
    };

    class secure_cell_seal: public secure_cell{
	public:
	    secure_cell_seal(const std::vector<uint8_t>& password):
		secure_cell(password){}

	    secure_cell& encrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end,  std::vector<uint8_t>::const_iterator context_begin, std::vector<uint8_t>::const_iterator context_end){
		size_t encrypted_length=0;
		if(themis_secure_cell_encrypt_seal(&_password[0], _password.size(), &(*context_begin), context_end-context_begin, &(*data_begin), data_end-data_begin, NULL, &encrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_encrypt_seal length determisnation error");
		_res.resize(encrypted_length);
		if(themis_secure_cell_encrypt_seal(&_password[0], _password.size(), &(*context_begin), context_end-context_begin, &(*data_begin), data_end-data_begin, &_res[0], &encrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_encrypt_seal error");
		return *this;
	    }

	    secure_cell& encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context){
		return encrypt(data.begin(), data.end(), context.begin(), context.end());
	    }

	    secure_cell& encrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end, const std::vector<uint8_t>& context){
		return encrypt(data_begin, data_end, context.begin(), context.end());
	    }

	    secure_cell& encrypt(const std::vector<uint8_t>& data, std::vector<uint8_t>::const_iterator context_begin, std::vector<uint8_t>::const_iterator context_end){
		return encrypt(data.begin(), data.end(), context_begin, context_end);
	    }


	    secure_cell& encrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end){
		std::vector<uint8_t> context(0);
		return encrypt(data_begin, data_end, context);
	    }

	    secure_cell& encrypt(const std::vector<uint8_t>& data){
		return encrypt(data.begin(), data.end());
	    }

	    secure_cell& decrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end,  std::vector<uint8_t>::const_iterator context_begin, std::vector<uint8_t>::const_iterator context_end){
		size_t decrypted_length=0;
		if(themis_secure_cell_decrypt_seal(&_password[0], _password.size(), &(*context_begin), context_end-context_begin, &(*data_begin), data_end-data_begin, NULL, &decrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_decrypt_seal length determisnation error");
		_res.resize(decrypted_length);
		if(themis_secure_cell_decrypt_seal(&_password[0], _password.size(), &(*context_begin), context_end-context_begin, &(*data_begin), data_end-data_begin, &_res[0], &decrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_decrypt_seal error");
		return *this;
	    }


	    secure_cell& decrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context){
		return decrypt(data.begin(), data.end(), context.begin(), context.end());
	    }

	    secure_cell& decrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end, const std::vector<uint8_t>& context){
		return decrypt(data_begin, data_end, context.begin(), context.end());
	    }
	    secure_cell& decrypt(const std::vector<uint8_t>& data, std::vector<uint8_t>::const_iterator context_begin, std::vector<uint8_t>::const_iterator context_end){
		return decrypt(data.begin(), data.end(), context_begin, context_end);
	    }

	    secure_cell& decrypt(std::vector<uint8_t>::const_iterator data_begin, std::vector<uint8_t>::const_iterator data_end){
		std::vector<uint8_t> context(0);
		return decrypt(data_begin, data_end, context);
	    }

	    secure_cell& decrypt(const std::vector<uint8_t> data){
		return decrypt(data.begin(), data.end());
	    }

    };

    class secure_cell_token_protect: public secure_cell{
	public:
	    secure_cell_token_protect(const std::vector<uint8_t>& password):
		secure_cell(password){}

	    secure_cell& encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context){
		size_t encrypted_length=0;
		size_t token_length=0;
		if(themis_secure_cell_encrypt_token_protect(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), NULL, &token_length, NULL, &encrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_encrypt_token_protect length determisnation error");
		_res.resize(encrypted_length);
		_token.resize(token_length);
		if(themis_secure_cell_encrypt_token_protect(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), &_token[0], &token_length, &_res[0], &encrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_encrypt_token_protect error");
		return *this;
	    }

	    secure_cell& encrypt(const std::vector<uint8_t> data){
		return encrypt(data, std::vector<uint8_t>(0));
	    }

	    secure_cell& decrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& token, const std::vector<uint8_t>& context){
		size_t decrypted_length=0;
		if(themis_secure_cell_decrypt_token_protect(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), &token[0], token.size(), NULL, &decrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_decrypt_token_protect length determisnation error");
		_res.resize(decrypted_length);
		if(themis_secure_cell_decrypt_token_protect(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), &token[0], token.size(), &_res[0], &decrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_decrypt_token_protect error");
		return *this;
	    }

	    secure_cell& decrypt(const std::vector<uint8_t> data, const std::vector<uint8_t>& token){
		return decrypt(data, token, std::vector<uint8_t>(0));
	    }

	    std::vector<uint8_t>& get_token(){return _token;}
    };

    class secure_cell_context_imprint: public secure_cell{
	public:
	    secure_cell_context_imprint(const std::vector<uint8_t>& password):
		secure_cell(password){}

	    secure_cell& encrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context){
		size_t encrypted_length=0;
		if(themis_secure_cell_encrypt_context_imprint(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), NULL, &encrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_encrypt_context_imprint length determisnation error");
		_res.resize(encrypted_length);
		if(themis_secure_cell_encrypt_context_imprint(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), &_res[0], &encrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_encrypt_context_imprint error");
		return *this;
	    }

	    secure_cell& decrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& context){
		size_t decrypted_length=0;
		if(themis_secure_cell_decrypt_context_imprint(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), NULL, &decrypted_length)!=THEMIS_BUFFER_TOO_SMALL)
		    throw themispp::exception("themis_secure_cell_encryp_context_imprint length determisnation error");
		_res.resize(decrypted_length);
		if(themis_secure_cell_decrypt_context_imprint(&_password[0], _password.size(), &context[0], context.size(), &data[0], data.size(), &_res[0], &decrypted_length)!=THEMIS_SUCCESS)
		    throw themispp::exception("themis_secure_cell_encrypt_context_imprint error");
		return *this;
	    }
    };

}//themis