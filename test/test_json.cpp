/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

/** \file
 * Unit tests of the used boost::JSON functionality
 */

#include <gtest/gtest.h>
#include <boost/json.hpp>

using namespace boost::json;

TEST(TestJson, BasicFunctions) {
  object obj;                                                     // construct an empty object
  obj[ "pi" ] = 3.141;                                            // insert a double
  obj[ "happy" ] = true;                                          // insert a bool
  obj[ "name" ] = "Boost";                                        // insert a string
  obj[ "nothing" ] = nullptr;                                     // insert a null
  obj[ "answer" ].emplace_object()["everything"] = 42;            // insert an object with 1 element
  obj[ "list" ] = { 1, 0, 2 };                                    // insert an array with 3 elements
  obj[ "object" ] = { {"currency", "USD"}, {"value", 42.99} };    // insert an object with 2 elements
  std::cout << "OBJECT" << std::endl << obj << std::endl;

  value jv = {
      { "pi", 3.141 },
      { "happy", true },
      { "name", "Boost" },
      { "nothing", nullptr },
      { "answer", {
          { "everything", 42 } } },
      {"list", {1, 0, 2}},
      {"object", {
          { "currency", "USD" },
          { "value", 42.99 }
      } }
  };
  std::cout << "VALUE" << std::endl << jv << std::endl;

  value jv1 = parse( "[1, 2, 3]" );
  std::cout << "VALUE1" << std::endl << jv1 << std::endl;
}
TEST(TestJson, ParseJson) {
  // First generate
  object obj;
  obj[ "float_value" ] = 3.141;
  obj[ "bool_value" ] = true;
  obj[ "text_value" ] = "Boost";
  obj[ "null_value" ] = nullptr;
  obj[ "int_value" ] = -42;
  obj[ "uint_value" ] = static_cast<uint64_t>(40);

  auto json_text = serialize(obj);


  const auto val = parse(json_text);
  const auto& dest = val.get_object();
  std::cout << dest.size() << std::endl;
  for (auto& [key, value] : dest) {
    std::cout << key << ": " << value <<  " (" << value.kind() << ")"<< std::endl;
    std::cout << key << ": " << "IsUint64: " << value.is_uint64() <<  " (" << value.kind() << ")"<< std::endl;
  }



}