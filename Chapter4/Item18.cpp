#include <iostream>
#include <memory>

class Investment {
    public:
        Investment() 
        {
            std::cout << "Investment constructor called" << std::endl;
        }
        virtual ~Investment() = default;
};

class Stock:
  public Investment {
    public:
        Stock() 
        {
            std::cout << "Stock constructor called" << std::endl;
        }
        virtual ~Stock() = default;
  };

class Bond:
  public Investment {
    public:
        Bond() 
        {
            std::cout << "Bond constructor called" << std::endl;
        }
        virtual ~Bond() = default;
  };

class RealEstate:
  public Investment {
    public:
        RealEstate() 
        {
            std::cout << "RealEstate constructor called" << std::endl;
        }
        virtual ~RealEstate() = default;
  };

  std::unique_ptr<Investment> makeInvestment() {
    std::unique_ptr<Investment> pInv ;
    // some code to determine the type of investment to make
    int type = 1; // for example, this could be determined by some logic
    if (type == 1) {
        // pInv = new Stock(); will not compile as a raw pointer cannot be assigned to a unique_ptr
        pInv.reset(new Stock()); // using reset to assign a new object to the unique pointer C++11
    } else if (type == 2) {
        pInv = std::make_unique<Bond>(); // C++14
    } else {
        pInv = std::make_unique<RealEstate>();
    }
    return pInv;
  }

  auto customDeleter = [](Investment* p) {
      std::cout << "Custom deleter called" << std::endl;
      delete p;
  };

  // same function as above but the unique pointer it returns has a custom deleter
  std::unique_ptr<Investment, decltype(customDeleter)> makeInvestmentWithCustomDeleter() {
      return std::unique_ptr<Investment, decltype(customDeleter)>(
          new Stock(),
          customDeleter
      );
  }


int main()
{
    int* p = new int(10);
    std::cout<<*p<<std::endl;

    delete p;
    // p can be used after being deleted as the memory it points to is deallocated but the pointer itself still exists
    p = new int(5);
    std::cout<<*p<<std::endl;
    delete p;   // must be deleted again if reallocated

    auto investment = makeInvestment();
    std::cout << "==========================" << std::endl;
    auto investmentWithCustomDeleter = makeInvestmentWithCustomDeleter();
    std::cout << "==========================" << std::endl;
    std::shared_ptr<Investment> sharedInvestment = makeInvestment(); // unique_ptr can be converted into a shared_ptr

    return 0;
}
