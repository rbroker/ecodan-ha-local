/* Copyright © 2023 Richard Broker
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), 
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <new>
#include <limits>
#include <string>
#include <deque>
#include <esp32-hal-psram.h>

namespace psram
{
    bool exists()
    {
        static bool yes = psramFound();
        return yes;
    }

    bool is_non_zero()
    {
        static bool nonZero = ESP.getPsramSize() > 0;
        return nonZero;
    }

    bool initialize()
    {
        static bool initialized = psramInit();        
        return initialized;
    }

    template<class T>
    struct allocator
    {
        using value_type = T;

        allocator() noexcept
        {    
            initialize();        
        }
       
        template<class U> allocator(const allocator<U>&) noexcept
        {            
        }
        
        template<class U> bool operator==(const allocator<U>&) const noexcept
        {
            return true;
        }

        template<class U> bool operator!=(const allocator<U>&) const noexcept
        {
            return false;
        }

        T* allocate(const size_t n) const
        {
            T* p;

            if (n > (std::numeric_limits<size_t>::max() / sizeof(T)))            
                throw std::bad_array_new_length();
            
            p = static_cast<T*>(ps_malloc(n * sizeof(T)));

            if (!p) // Fallback to regular heap if psram allocation fails.
            {
               p = static_cast<T*>(malloc(n * sizeof(T)));
            }

            if (!p)            
                throw std::bad_alloc();

            return p;
        }

        void deallocate(T* const p, size_t) const noexcept
        {
            free(p);
        }
    };

    using string = std::basic_string<char, std::char_traits<char>, psram::allocator<char>>;
    using deque = std::deque<psram::string, allocator<psram::string>>;
}