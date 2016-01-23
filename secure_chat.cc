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

#include <cstdio>
#include <string>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/directory_entry.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"
#include "ppapi/cpp/websocket.h"
#include "nacl_io/nacl_io.h"

#include "helpers/base64.hpp"
#include "pnacl_websocket_api.hpp"
#include "pnacl_fileio_api.hpp"
#include "themispp/secure_keygen.hpp"
#include "themispp/secure_rand.hpp"


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

class secure_chat_instance : public pp::Instance, public pnacl::fileio_listener_t, public pnacl::web_socket_receive_listener{
public:
  explicit secure_chat_instance(PP_Instance instance) : 
    pp::Instance(instance),
    store_(this,this),
    socket_(this, this),
    url_(""),
    user_name_(""){
	nacl_io_init_ppapi(instance, pp::Module::Get()->get_browser_interface());
    }

  virtual ~secure_chat_instance() {}
  
  virtual bool Init(uint32_t argc, const char * argn[], const char * argv[]) {
    for(uint32_t i=0;i<argc;++i){
      if(strcmp(argn[i], "url")==0){
	url_=argv[i];
      }
      if(strcmp(argn[i], "server_pub")==0){
	server_pub_=argv[i];
      }
    }
    if(url_=="" || server_pub_==""){
      return false;
    }
    return true;
  }
  
private:
  pnacl::websocket_api socket_;
  pnacl::fileio_api store_;
  std::string url_;
  std::string user_name_;
  std::string server_pub_;
  std::vector<uint8_t> private_key_;
  std::vector<uint8_t> public_key_;
  std::string room_name_;
  std::vector<uint8_t>  room_key_;
  std::vector<std::string> invites_;
  themispp::secure_cell_seal* room_encrypter_;


private:
  void on_new_room(const std::string& new_room_key){
    room_name_=new_room_key;
    store_.mk_sub_key(new_room_key);
    store_.save(new_room_key+"/pub", public_key_);
    store_.save(new_room_key+"/priv", private_key_);
    themispp::secure_rand<32> rnd;
    room_key_=rnd.get();
    store_.save(new_room_key+"/key", room_key_);
    room_encrypter_=new themispp::secure_cell_seal(room_key_);
    post("new_room", new_room_key);
  }

  void on_room_ready(const std::string& history)
  {
    post("open_room", room_name_);
  }
  
  void on_room_load(){
    if(!public_key_.empty() && !private_key_.empty() && !room_key_.empty()){
      room_encrypter_=new themispp::secure_cell_seal(room_key_);
      socket_.send(std::string("PUBKEY ")+webthemispp::base64_encode(public_key_));
      socket_.send(std::string("OPENROOM ")+room_name_);
    }
  }

  void remove_room(const std::string& room_name){
    store_.rm_key(room_name);
  }
public:  
  virtual void on_receive(const std::string& data){
    std::string command;
    std::istringstream st(data);
    postInfo(data);
    st>>command;
    if(command=="ERROR"){
      st.ignore(1);
      postError(st.str());
    }else if(command=="ERRORROOM"){
      std::string new_room_name;
      st>>new_room_name;
      remove_room(new_room_name);
    }else if(command=="NEWROOM"){
      std::string new_room_name;
      st>>new_room_name;
      on_new_room(new_room_name);
    }else if(command=="OPENROOM"){
      std::string history=st.str().substr(9, st.str().length()+1-9);
      on_room_ready(history);
    }
    else if(command=="INVITE"){
      std::string pub_key, my_pub_key, room, invite;
      st>>pub_key>>room>>my_pub_key>>invite;
      std::vector<std::string>::iterator it=std::find(invites_.begin(), invites_.end(), invite);
      if(it!=invites_.end()){
	socket_.send(std::string("INVITE_RESP ")+pub_key+" "+room+" "+webthemispp::base64_encode(room_key_));
	invites_.erase(it);
      }
    }else if(command=="INVITE_RESP"){
      std::string pub_key, room, room_key;
      st>>pub_key>>room>>room_key;;
      room_name_=room;
      store_.mk_sub_key(room_name_);
      store_.save(room_name_+"/pub", public_key_);
      store_.save(room_name_+"/priv", private_key_);
      room_key_=webthemispp::base64_decode(room_key);
      store_.save(room_name_+"/key", room_key_);
      on_room_load();
    }else if(command=="MESSAGE"){
      std::string room, ms;
      st>>room>>ms;
      std::vector<uint8_t> msv=room_encrypter_->decrypt(webthemispp::base64_decode(ms)).get();
      post("new_message", std::string(msv.begin(), msv.end()));
    }else {
      postError("receive malformed response from server");
    }
  }
  virtual void on_connect(){
    postInfo("connected to server");
  }
  virtual void on_error(int32_t code, const std::string& reason){
    post("connection_lost", "reconnecting...");
    postError("connecion lost");
    socket_.open(url_);
  }
  
public:
  virtual void on_open(const int32_t error_code, const std::string& chat_list){
    if(error_code!=PNACL_IO_SUCCESS){
      std::ostringstream s;
      s<<"can't open storage "<<std::hex<<error_code;
      postError(s.str());
      return;
    }
    post("available_chat_list", chat_list);
  }

  virtual void on_load(const int32_t error_code, const std::string& key, const std::vector<uint8_t>& data){
    if(error_code!=PNACL_IO_SUCCESS)
      postError(key + " can`t load");
    else{
      std::string kk=key.substr(33,key.length()+1-33);
      postInfo(kk+" loaded");
      if(kk=="pub")
	public_key_=data;
      else if (kk == "priv")
	private_key_=data;
      else if (kk=="key")
	room_key_=data;
      on_room_load();
    }
  }

  virtual void on_save(const int32_t error_code){
    if(error_code!=PNACL_IO_SUCCESS)
      postError("storage error");
  }
private:
  void create_new_room()
  {
    themispp::key_pair_generator_t<themispp::EC> gen;
    public_key_=gen.get_pub();
    private_key_=gen.get_priv();
    socket_.send(std::string("PUBKEY ")+webthemispp::base64_encode(gen.get_pub()));
    socket_.send(std::string("NEWROOM"));
  }

  void open_room(const std::string& room_to_open){
    room_name_=room_to_open;
    postInfo(room_to_open);
    public_key_.clear();
    private_key_.clear();
    room_key_.clear();
    store_.load(room_to_open+"/pub");
    store_.load(room_to_open+"/priv");
    store_.load(room_to_open+"/key");
  }

  void generate_invite()
  {
    themispp::secure_rand<32> rnd;
    std::vector<uint8_t> new_invite=rnd.get();
    //store_.save(room_name_+"/"+webthemispp::base64_encode(new_invite), std::vector<uint8_t>(0));
    invites_.push_back(webthemispp::base64_encode(new_invite));
    post("invite", room_name_+" "+webthemispp::base64_encode(public_key_)+" "+webthemispp::base64_encode(new_invite));
  }

  void join_by_invite(const std::string& room_name, const std::string& invite_owner_pub_key, const std::string& invite){
    themispp::key_pair_generator_t<themispp::EC> gen;
    public_key_=gen.get_pub();
    private_key_=gen.get_priv();
    post("pub_key",webthemispp::base64_encode(public_key_));
    socket_.send(std::string("PUBKEY ")+webthemispp::base64_encode(public_key_));
    socket_.send("INVITE "+webthemispp::base64_encode(public_key_)+" "+room_name+" "+invite_owner_pub_key+" "+invite);
  }
  
public:
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string()){
      postError("non string messages not supported");
      return;
    }
    std::istringstream message(var_message.AsString());
    std::string operation;
    message>>operation;
    if (operation == "login"){
      std::string password;
      message>>user_name_>>password;
      store_.open(webthemispp::base64_encode((uint8_t*)((url_+user_name_).c_str()), (url_+user_name_).length()), password);
      socket_.open(url_);
    } else if (operation == "create_new_room"){
      create_new_room();
    }else if (operation == "open_room"){
      std::string room_to_open;
      message>>room_to_open;
      open_room(room_to_open);
    }else if (operation == "generate_invite"){
      generate_invite();
    }else if (operation == "join_by_invite"){
      std::string room_name, invite_owner_pub_key, invite;
      message>>room_name>>invite_owner_pub_key>>invite;
      join_by_invite(room_name, invite_owner_pub_key, invite);
    }else if (operation == "message"){
      std::string m=message.str().substr(8,message.str().length()-1+8);
      socket_.send("MESSAGE "+room_name_+" "+webthemispp::base64_encode(room_encrypter_->encrypt(std::vector<uint8_t>(m.c_str(), m.c_str()+m.length()+1)).get()));
    }else
      postError(std::string("openation <i>")+operation+"</i> not supported");
  }

private:
  void post(const std::string& command, const std::string& param1, const std::string& param2=""){
    pp::VarArray message;
    message.Set(0,command);
    message.Set(1,param1);
    if(param2 != ""){
      message.Set(2,param2);
    }
    PostMessage(message);
  }

  void postError(const std::string& message, const std::string& details=""){
    post("error", message, details);
  }

  void postInfo(const std::string& message, const std::string& details=""){
    post("info", message, details);
  }
};


class secure_chat_module : public pp::Module {
public:
  secure_chat_module() : pp::Module() {}
  virtual ~secure_chat_module() {}
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new secure_chat_instance(instance);
  }
};

namespace pp {
    Module* CreateModule() {
	return new secure_chat_module();
    }
} // namespace pp
