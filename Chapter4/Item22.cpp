#include "Item22.h"
#include "gadget.h"
#include <string>
#include <vector>

struct Widget::Impl {          // definition of Widget::Impl
  std::string name;            // with data members formerly
  std::vector<double> data;    // in Widget
  Gadget g1, g2, g3;
};


Widget::Widget()                    // per Item 21, create
: pImpl(std::make_unique<Impl>())   // std::unique_ptr
{}                                  // via std::make_unique

Widget::~Widget() = default;        // the destructors must be defined here so that at compilation Impl is a complete type otherwise we will get this error error: invalid application of ‘sizeof’ to incomplete type ‘Widget::Impl’ static_assert(sizeof(_Tp)>0,

Widget::Widget(Widget && rhs) = default; // same as destructors must be defined here

Widget& Widget::operator=(Widget && rhs) = default; // same as destructors must be defined here

Widget::Widget(const Widget& rhs)              // copy ctor
: pImpl(nullptr)
{ if (rhs.pImpl) pImpl = std::make_unique<Impl>(*rhs.pImpl); }

Widget& Widget::operator=(const Widget& rhs)   // copy operator=
{
  if (!rhs.pImpl) pImpl.reset();
  else if (!pImpl) pImpl = std::make_unique<Impl>(*rhs.pImpl);
  else *pImpl = *rhs.pImpl;

  return *this;
}