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

#ifndef _PNACL_FILEIO_API_HPP_
#define _PNACL_FILEIO_API_HPP_

#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/directory_entry.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "json/json.h"

#include "themispp/exception.hpp"
#include "themispp/secure_cell.hpp"
#include "exception.hpp"
#include "helpers/base64.hpp"

#include <sstream>

#define PNACL_IO_SUCCESS 0xffff0000
#define PNACL_IO_OPEN_ERROR PNACL_IO_SUCCESS^1
#define PNACL_IO_SAVE_ERROR PNACL_IO_SUCCESS^2
#define PNACL_IO_LOAD_ERROR PNACL_IO_SUCCESS^3
#define PNACL_IO_STORAGE_NOT_READY_ERROR PNACL_IO_SUCCESS^4
#define PNACL_IO_KEY_NOT_FOUND_ERROR PNACL_IO_SUCCESS^5
#define PNACL_IO_INVALID_PASSWORD_ERROR PNACL_IO_SUCCESS^6


namespace pnacl {
  
  class fileio_api{
  private:
    typedef std::function<void(Json::Value&)> load_callback_t;
    typedef std::function<void(int)> error_callback_t;
    typedef std::function<void()> open_callback_t;
    typedef std::function<void()> save_callback_t;
    typedef std::function<void()> delete_callback_t;

  public:
    fileio_api(pp::Instance *instance,
	       const std::string& password,
	       open_callback_t on_open,
	       error_callback_t on_error):
      instance_(instance),
      file_system_(instance, PP_FILESYSTEMTYPE_LOCALPERSISTENT),
      callback_factory_(this),
      file_thread_(instance),
      file_system_ready_(false),
      encrypter_(std::vector<uint8_t>(password.data(), password.data()+password.length())){
      file_thread_.Start();
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::open_file_system_, on_open, on_error));
    }
    
    virtual ~fileio_api(){}
    
    void save(const std::string& key, const Json::Value& data, save_callback_t on_save, error_callback_t on_error) {
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::save_, key, data, on_error));
    }

    void remove(const std::string& key, delete_callback_t on_remove, error_callback_t on_error){
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::rm_, key, on_remove, on_error));
    }
    
    void load(const std::string& key, load_callback_t on_load, error_callback_t on_error) {
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::load_, key, on_load, on_error));
    }

  private:
    void open_file_system_(int32_t result, open_callback_t on_open, error_callback_t on_error) {
      int32_t res=file_system_.Open(1024 * 1024, pp::BlockUntilComplete());
      if (res!=PP_OK){
	on_error(res);
	return;
      }
      file_system_ready_=true;
      on_open();
    }

    void rm_(int32_t result, const std::string& file, delete_callback_t on_remove, error_callback_t on_error){
      if (!file_system_ready_)
	on_error(-1);
      on_remove();
    }

    void save_(int32_t /* result */, const std::string& file_name, const Json::Value& context, error_callback_t on_error) {
      if (!file_system_ready_)
	on_error(-1);
      std::vector<uint8_t> enc_data;
      try{
	std::string string_context=Json::FastWriter().write(context);
	enc_data=encrypter_.encrypt(std::vector<uint8_t>(string_context.c_str(), string_context.c_str()+string_context.length()));
      }catch(themispp::exception_t& e){
	on_error(-5);
      }
      pp::FileRef ref(file_system_, file_name.c_str());
      pp::FileIO file(instance_);
      if (file.Open(ref, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_TRUNCATE, pp::BlockUntilComplete()) == PP_OK
	  && file.Write(0, (const char*)(&enc_data[0]), enc_data.size(), pp::BlockUntilComplete())==enc_data.size()
	  && file.Flush(pp::BlockUntilComplete())==PP_OK)
	int a=1;//on_save();
      else
	on_error(-6);
    }

    void load_(int32_t result, const std::string& file_name, load_callback_t on_load, error_callback_t on_error){
      if (!file_system_ready_)
	on_error(-1);
      pp::FileRef ref(file_system_, file_name.c_str());
      pp::FileIO file(instance_);
      PP_FileInfo info;
      int32_t open_result = file.Open(ref, PP_FILEOPENFLAG_READ, pp::BlockUntilComplete());
      if (open_result == PP_ERROR_FILENOTFOUND)
	on_error(-2);
      else if (open_result == PP_OK && file.Query(&info, pp::BlockUntilComplete())==PP_OK){
	std::vector<uint8_t> data(info.size);
	if(file.Read(0, (char*)(&data[0]), data.size(), pp::BlockUntilComplete())==data.size())
	  try{
	    data=encrypter_.decrypt(data);
	    Json::Value root;
	    Json::Reader reader;
	    if(!reader.parse(std::string((char*)(&data[0]), data.size()), root))
	      on_error(-4);
	    on_load(root);
	    return;
	  }catch(themispp::exception_t& e){
	    on_error(-3);
	    return;
	  }
      }
      on_error(open_result);
    }	    

  private:
    pp::FileSystem file_system_;
    pp::Instance* instance_;
    pp::CompletionCallbackFactory<fileio_api> callback_factory_;
    bool file_system_ready_;
    pp::SimpleThread file_thread_;
    //    std::string user_name_;
    themispp::secure_cell_seal_t encrypter_;
    //    fileio_listener_t *listener_;
  };
  
} //end pnacl


#endif /* _PNACL_FILEIO_API_HPP_ */
