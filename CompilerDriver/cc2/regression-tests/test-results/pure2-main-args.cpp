
#define CPP2_IMPORT_STD          Yes

//=== Cpp2 type declarations ====================================================


#include "cpp2util.h"

#line 1 "pure2-main-args.cpp2"


//=== Cpp2 type definitions and function declarations ===========================

#line 1 "pure2-main-args.cpp2"
auto main(int const argc_, char** argv_) -> int;

//=== Cpp2 function definitions =================================================

#line 1 "pure2-main-args.cpp2"
auto main(int const argc_, char** argv_) -> int { 
    auto const args = cpp2::make_args(argc_, argv_); 
#line 2 "pure2-main-args.cpp2"
    std::cout 
        << "args.argc            is " + cpp2::to_string(args.argc) + "\n" 
        << "args.argv[0]         is " + cpp2::to_string(CPP2_ASSERT_IN_BOUNDS(args.argv, 0)) + "\n"; }
