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

#ifndef PNACL_EXCEPTION_HPP
#define PNACL_EXCEPTION_HPP

#include <stdexcept>

namespace pnacl{
    class exception: public std::runtime_error{
	public:
	explicit exception(const std::string& what):
	    std::runtime_error(what.c_str()){}
    };

    class file_not_found_exception: public pnacl::exception{
	public:
	explicit file_not_found_exception(const std::string& what):
	    pnacl::exception(what.c_str()){}
    };
}//themis ns

#endif