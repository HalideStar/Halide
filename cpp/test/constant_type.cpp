#include <stdio.h>
#include <Halide.h>

using namespace Halide;

template<typename T>
void test_type() {
    Func f;
    Var x;
    f(x) = cast<T>(1);
    Image<T> im(10);
    f.realize(im);
    Expr add_one = im() + 1;
    if (add_one.type() != type_of<T>()) {
        std::cout << "Add 1 changed type from " << type_of<T>() << " to " << add_one.type() << "\n";
    }
    Expr one_add = im() + 1;
    if (one_add.type() != type_of<T>()) {
        std::cout << "Pre-add 1 changed type from " << type_of<T>() << " to " << one_add.type() << "\n";
    }
    Expr add_exp = im() + (Expr(1) + 1);
    if (add_exp.type() != type_of<T>()) {
        std::cout << "Add constant expression changed type from " << type_of<T>() << " to " << add_exp.type() << "\n";
    }
}

int main(int argc, char **argv) {
    test_type<uint8_t>();
    test_type<uint16_t>();
    test_type<uint32_t>();
    test_type<int8_t>();
    test_type<int16_t>();
    test_type<int32_t>();
    test_type<float>();
    test_type<double>();
    
    printf("Success!\n");
    return 0;
}
