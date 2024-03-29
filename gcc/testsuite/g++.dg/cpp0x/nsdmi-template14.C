// PR c++/58583
// { dg-do compile { target c++11 } }

template<int> struct A // { dg-error "has been parsed" }
{
  int i = (A<0>(), 0); // { dg-error "has been parsed" }
};

template<int N> struct B
{
  B* p = new B<N>;
};

B<1> x; // { dg-error "recursive instantiation of non-static data" }

struct C
{
  template<int N> struct D
  {
    D* p = new D<0>;
  };
};
