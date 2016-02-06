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

  class fileio_listener_t{
  public:
    virtual void on_error(const in)
    virtual void on_load(const int32_t error_code, const std::vector<uint8_t>& data)=0;
    virtual void on_save(const int32_t error_code)=0;
  };
  
  class fileio_api{
  public:
    fileio_api(pp::Instance *instance, fileio_listener_t *listener):
      instance_(instance),
      file_system_(instance, PP_FILESYSTEMTYPE_LOCALPERSISTENT),
      callback_factory_(this),
      file_thread_(instance),
      file_system_ready_(false),
      encrypter_(NULL),
      user_name_(""),
      listener_(listener){
      file_thread_.Start();
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::open_file_system));
    }
    
    virtual ~fileio_api(){
      if(encrypter_)
	delete encrypter_;
    }
    
    void save(const std::string& key, const std::vector<uint8_t>& data) {
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::save_, key, data));
    }

    void remove(const std::string& key){
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::rm_, key));
    }
    
    void load(const std::string& key, const std::string& password) {
      try{
	encrypter_=new themispp::secure_cell_seal_t(std::vector<uint8_t>(password.data(), password.data()+password.length()));
      }catch(themispp::exception_t& e){
	listener_->on_open(PNACL_IO_LOAD_ERROR,"");
	return;
      }
      file_thread_.message_loop().PostWork(callback_factory_.NewCallback(&fileio_api::load_));
    }

  private:
    void rm_(int32_t result, const std::string& key){
      if (!file_system_ready_ && !encrypter_)
	listener_->on_save(PNACL_IO_STORAGE_NOT_READY_ERROR);
      pp::FileRef ref(file_system_, (std::string("/")+user_name_+"/"+key).c_str());
      ref.ReadDirectoryEntries(callback_factory_.NewCallbackWithOutput(&fileio_api::list_rm_, ref));
    }

    void open_file_system(int32_t result) {
      fprintf(stderr, "open_file_system\n");
      if(!file_system_ready_){
	int32_t res=file_system_.Open(1024 * 1024, pp::BlockUntilComplete());
	if (res!=PP_OK){
	  listener_->on_open(res,"");
	  return;
	}
	file_system_ready_=true;
      }
    }
    void save_(int32_t /* result */, const std::string& file_name, const std::vector<uint8_t>& context) {
      if (!file_system_ready_ && !encrypter_)
	listener_->on_save(PNACL_IO_STORAGE_NOT_READY_ERROR);
      std::vector<uint8_t> enc_data;
      try{
	enc_data=encrypter_->encrypt(context);
      }catch(themispp::exception_t& e){
	listener_->on_save(PNACL_IO_SAVE_ERROR);
      }
      pp::FileRef ref(file_system_, (std::string("/")+user_name_+"/"+file_name).c_str());
      pp::FileIO file(instance_);
      if (file.Open(ref, PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_TRUNCATE, pp::BlockUntilComplete()) == PP_OK
	  && file.Write(0, (const char*)(&enc_data[0]), enc_data.size(), pp::BlockUntilComplete())==enc_data.size()
	  && file.Flush(pp::BlockUntilComplete())==PP_OK)
	listener_->on_save(PNACL_IO_SUCCESS);
      else
	listener_->on_save(PNACL_IO_SAVE_ERROR);
    }

    void load_(int32_t result){
      if (!file_system_ready_ && !encrypter_)
	listener_->on_load(PNACL_IO_STORAGE_NOT_READY_ERROR, std::vector<uint8_t>(0));
      pp::FileRef ref(file_system_, (std::string("/")+user_name_+"/"+file_name).c_str());
      pp::FileIO file(instance_);
      PP_FileInfo info;
      int32_t open_result = file.Open(ref, PP_FILEOPENFLAG_READ, pp::BlockUntilComplete());
      if (open_result == PP_ERROR_FILENOTFOUND)
	listener_->on_open(PNACL_IO_KEY_NOT_FOUND_ERROR, std::vector<uint8_t>(0));
      else if (open_result == PP_OK && file.Query(&info, pp::BlockUntilComplete())==PP_OK){
	std::vector<uint8_t> data(info.size);
	if(file.Read(0, (char*)(&data[0]), data.size(), pp::BlockUntilComplete())==data.size())
	  try{
	    data=encrypter_->decrypt(data);
	    listener_->on_load(PNACL_IO_SUCCESS, data);
	    return;
	  }catch(themispp::exception_t& e){
	    listener_->on_load(PNACL_IO_INVALID_PASSWORD_ERROR, std::vector<uint8_t>(0));
	    return;
	  }
      }
      listener_->on_load(PNACL_IO_LOAD_ERROR, std::vector<uint8_t>(0));
    }	    

  private:
    pp::FileSystem file_system_;
    pp::Instance* instance_;
    pp::CompletionCallbackFactory<fileio_api> callback_factory_;
    bool file_system_ready_;
    pp::SimpleThread file_thread_;
    std::string user_name_;
    themispp::secure_cell_seal_t* encrypter_;
    fileio_listener_t *listener_;
  };
  
} //end pnacl


#endif /* _PNACL_FILEIO_API_HPP_ */
