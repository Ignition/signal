#include <iostream>
#include <functional>
#include <tuple>
#include <unordered_set>
#include <optional>
#include <sstream>

template <typename T>
struct signal
{
    using result_t = T;

    explicit signal(T val) : val_{std::move(val)} {}

    [[nodiscard]] auto get() const -> T const &
    {
      std::cout << "get" << std::endl;
      return val_;
    }

    void set(T val)
    {
      std::cout << "set" << std::endl;
      val_         = std::move(val);
      auto visited = std::unordered_set<void *>{};
      for (auto & func: dirty_functions_) { func(visited); }
    }

    auto reg(std::function<void(std::unordered_set<void *> &)> func)
    {
      dirty_functions_.emplace_back(std::move(func));
    }

private:
    T                                                              val_;
    std::vector<std::function<void(std::unordered_set<void *> &)>> dirty_functions_;
};

template <typename Func, typename...Args>
struct computable
{
    using result_t = std::invoke_result_t<Func, typename Args::result_t...>;

    explicit computable(Func func, Args & ... signals)
      : signals_{std::addressof(signals)...}, func_{std::move(func)}
    {
      (signals.reg([this](std::unordered_set<void *> & visited)
                   {
                       if (!visited.contains(this))
                       {
                         std::cout << "dirty" << std::endl;
                         value_.reset();
                         visited.insert(this);
                         for (auto & f: dependants_) f(visited);
                       }
                   }), ...);
    }

    auto get() -> result_t const &
    {
      std::cout << "get" << std::endl;
      if (!value_)
      {
        std::cout << "compute" << std::endl;
        value_.emplace(std::apply([this](auto...args)
                                  {
                                      return std::invoke(func_, args->get()...);
                                  }, signals_));
      }
      return *value_;

    }

    auto reg(std::function<void(std::unordered_set<void *> &)> func)
    {
      dependants_.emplace_back(std::move(func));
    }

private:

    std::tuple<Args * ...>                                         signals_;
    Func                                                           func_;
    std::optional<result_t>                                        value_;
    std::vector<std::function<void(std::unordered_set<void *> &)>> dependants_;
};


int main()
{
  auto s1 = signal{1};
  auto c1 = computable{[](int a) { return a + 1; }, s1};
  auto c2 = computable{[](int a) { return a + 2; }, c1};

  std::cout << c2.get() << std::endl;


  s1.set(2);
  std::cout << c2.get() << std::endl;

  auto s2 = signal{std::string{"hello"}};

  auto c3 = computable{[](int a, std::string const & b)
                       {
                           std::stringstream ss;
                           ss << b << a;
                           return ss.str();
                       }, c1, s2};

  std::cout << c3.get() << std::endl;
}
