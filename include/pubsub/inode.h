/*
 * Copyright 2024 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <string>
namespace pub_sub {
    class INode {
    public:
        void Name(const std::string& name) {name_ = name;}
        [[nodiscard]] const std::string& Name() const { return name_;}
    private:
        std::string name_;
    };

} // pub_sub

