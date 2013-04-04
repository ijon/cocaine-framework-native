#ifndef COCAINE_FRAMEWORK_BASE_HPP
#define COCAINE_FRAMEWORK_BASE_HPP

#include <memory>

namespace cocaine { namespace framework {

template<class T>
class self_managed :
    public std::enable_shared_from_this<T>
{
public:
    self_managed() :
        m_self(static_cast<T*>(this))
    {
        // pass
    }

    virtual
    ~self_managed() {
        if (m_self) {
            m_self.reset();
        }
    }

    std::shared_ptr<T>
    take_control() {
        std::shared_ptr<T> new_controller;
        m_self.swap(new_controller);
        return new_controller;
    }

    using std::enable_shared_from_this<T>::shared_from_this;

    template<class U>
    std::shared_ptr<U>
    shared_from_this() {
        return std::dynamic_pointer_cast<U>(shared_from_this());
    }

private:
    std::shared_ptr<T> m_self;
};

}} // cocaine::framework

#endif // COCAINE_FRAMEWORK_BASE_HPP
