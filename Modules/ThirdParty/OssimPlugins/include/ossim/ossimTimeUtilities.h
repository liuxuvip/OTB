/*
 * Copyright (C) 2005-2019 by Centre National d'Etudes Spatiales (CNES)
 *
 * This file is licensed under MIT license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef ossimTimeUtilities_h
#define ossimTimeUtilities_h

#include "ossim/ossimStringUtilities.h"
#include "ossim/ossimOperatorUtilities.h"
#include "ossimPluginConstants.h"
#include <cassert>
#include <ostream>

#define USE_BOOST_TIME 1

class ossimDate;

namespace ossimplugins { namespace time {
   // class ModifiedJulianDate;
   // class Duration;
   namespace details
   {
      class DayFrac
      {
      public:
         typedef double scalar_type;
         // DayFrac(DayFrac const&) = default;
         // DayFrac(DayFrac &&) = default;
         // DayFrac& operator=(DayFrac const&) = default;
         // DayFrac& operator=(DayFrac &&) = default;
         double as_day_frac() const { return m_day_frac; }

         std::ostream & display(std::ostream & os) const { return os << m_day_frac; }
         std::istream & read   (std::istream & is)       { return is >> m_day_frac; }

      protected:
         /**@name Construction/destruction
         */
         //@{
         /** Initialization constructor.
         */
         explicit DayFrac() {} // = default;
         explicit DayFrac(double day_frac) : m_day_frac(day_frac) {}
         /** Protected destructor.
         */
         ~DayFrac() {}// = default;
         //@}

         /**@name Operations
         */
         //@{
         void add(DayFrac const& rhs) { m_day_frac += rhs.m_day_frac; }
         void sub(DayFrac const& rhs) { m_day_frac -= rhs.m_day_frac; }
         void mult(scalar_type coeff) { m_day_frac *= coeff; }
         void div(scalar_type coeff)  { assert(coeff && "Cannot divide by 0"); m_day_frac /= coeff; }
         template <typename V> friend scalar_type ratio_(V const& lhs, V const& rhs)
         { return lhs.as_day_frac() / rhs.as_day_frac(); }

         template <typename U, typename V> friend U& operator+=(U & u, V const& v) {
            u.add(v);
            return u;
         }
         template <typename U, typename V> friend U& operator-=(U & u, V const& v) {
            u.sub(v);
            return u;
         }

         template <typename U, typename V> static U diff(V const& lhs, V const& rhs) {
            U const res(lhs.as_day_frac() - rhs.as_day_frac());
            return res;
         }

         template <typename U> friend U& operator*=(U & u, scalar_type const& v) {
            u.mult(v);
            return u;
         }
         template <typename U> friend U& operator/=(U & u, scalar_type const& v) {
            u.div(v);
            return u;
         }

         template <typename T> friend bool operator<(T const& lhs, T const& rhs) {
            return lhs.as_day_frac() < rhs.as_day_frac();
         }
         template <typename T> friend bool operator==(T const& lhs, T const& rhs) {
            return lhs.as_day_frac() == rhs.as_day_frac();
         }
         //@}
      private:
         double m_day_frac;
      };
   }

   /**
    * Duration abstraction.
    *
    * Values of this class represent time interval.
    *
    * <p><b>Semantics</b><br>
    * <li> Value, mathematical: it's relative position
    * <li> Time interval
    *
    * @see \c std::duration<>
    */
   class Duration
      : public details::DayFrac
      , private addable<Duration>
      , private substractable<Duration>
      , private streamable<Duration>
      , private multipliable2<Duration, double>
      , private dividable<Duration, details::DayFrac::scalar_type>
      , private equality_comparable<Duration>
      , private less_than_comparable<Duration>
      {
      public:
         typedef details::DayFrac::scalar_type scalar_type;

         /**@name Construction/destruction
         */
         //@{
         /** Initialization constructor.
         */
         Duration() {} // = default;
         explicit Duration(double day_frac)
            : details::DayFrac(day_frac) {}
         //@}

         double total_seconds() const {
            return as_day_frac() * 24 * 60 * 60;
         }
         double total_microseconds() const {
            return total_seconds() * 1000 * 1000;
         }
         bool is_negative() const { return as_day_frac() < 0.0; }
         Duration invert_sign() { return Duration(- as_day_frac()); }
         friend Duration abs(Duration const& d) { return Duration(std::abs(d.as_day_frac())); }
      };

   /**
    * Modified JulianDate abstraction.
    *
    * Objects of this class represent points in time.
    *
    * <p><b>Semantics</b><br>
    * <li> Value, mathematical: it's an absolute position
    * <li> Point in time
    * @see \c std::time_point<>
    */
   class ModifiedJulianDate
      : public details::DayFrac
      , private addable<ModifiedJulianDate, Duration>
      , private substractable<ModifiedJulianDate, Duration>
      , private substractable_asym<Duration, ModifiedJulianDate>
      , private streamable<ModifiedJulianDate>
      , private equality_comparable<ModifiedJulianDate>
      , private less_than_comparable<ModifiedJulianDate>
      {
      public:
         typedef details::DayFrac::scalar_type scalar_type;

         /**@name Construction/destruction
         */
         //@{
         /** Initialization constructor.
         */
         ModifiedJulianDate() {} // = default;
         explicit ModifiedJulianDate(double day_frac)
            : details::DayFrac(day_frac) {}
         //@}
         using details::DayFrac::diff;
      };

   OSSIM_PLUGINS_DLL ModifiedJulianDate toModifiedJulianDate(string_view const& utcTimeString);
   inline Duration microseconds(double us) {
      return Duration(us / (24ULL * 60 * 60 * 1000 * 1000));
   }
   inline Duration seconds(double us) {
      return Duration(us / (24ULL * 60 * 60));
   }
   OSSIM_PLUGINS_DLL std::string to_simple_string(ModifiedJulianDate const& d);
   OSSIM_PLUGINS_DLL std::string to_simple_string(Duration const& d);

   namespace details {
      // strptime is not portable, hence this simplified emulation
      OSSIM_PLUGINS_DLL ossimDate strptime(string_view const& format, string_view const& date);
   } // details namespace

} } // ossimplugins namespace::time

#if defined(USE_BOOST_TIME)


// Use nanosecond precision in boost dates and durations
#define BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
#  include <boost/config.hpp>
#  include <boost/date_time/posix_time/posix_time.hpp>

namespace boost { namespace posix_time {
   class precise_duration;
   double ratio_(precise_duration const& lhs, precise_duration const& rhs);




   class precise_duration
      : private ossimplugins::addable<precise_duration>
      , private ossimplugins::substractable<precise_duration>
      , private ossimplugins::streamable<precise_duration>
      , private ossimplugins::multipliable2<precise_duration, double>
      , private ossimplugins::dividable<precise_duration, double>
      , private ossimplugins::equality_comparable<precise_duration>
      , private ossimplugins::less_than_comparable<precise_duration>
      , private ossimplugins::addable<ptime, precise_duration>
      , private ossimplugins::substractable<ptime, precise_duration>
      {
public:
        using InternalDurationType = boost::posix_time::time_duration;

        precise_duration() = default;
        precise_duration(InternalDurationType const& d): m_duration(d) {}
precise_duration(double us ): m_duration(boost::posix_time::nanoseconds(static_cast<long long>(std::floor(us * 1e3)))){}


         double total_seconds() const {
            return m_duration.total_nanoseconds() / 1000000000.;
         }
         double total_microseconds() const {
            return m_duration.total_nanoseconds() / 1000.;
         }
         double total_nanoseconds() const {
            return m_duration.total_nanoseconds();
         }
         
         bool is_negative() const { return total_nanoseconds() < 0.0; }
         precise_duration invert_sign() { return precise_duration(- total_microseconds()); }
         std::ostream & display(std::ostream & os) const { return os << m_duration; }
         std::istream & read   (std::istream & is)       { return is >> m_duration; }

  friend precise_duration& operator+=(precise_duration & u, precise_duration const& v)
  {
    u.m_duration += v.m_duration;
    return u;
  }

  friend precise_duration& operator-=(precise_duration & u, precise_duration const& v)
  {
    u.m_duration -= v.m_duration;
    return u;
  }

  friend precise_duration& operator*=(precise_duration & u, double v)
  {
    u.m_duration = boost::posix_time::nanoseconds(static_cast<long long>(std::round(
                            u.m_duration.total_nanoseconds() * v)));
    return u;
  }


  friend precise_duration& operator/=(precise_duration & u, double v)
  {
    u.m_duration = boost::posix_time::nanoseconds(static_cast<long long>(std::round(
                            u.m_duration.total_nanoseconds() / v)));
    return u;
  }

  friend bool operator < (precise_duration const& lhs, precise_duration const& rhs)
  {
    return lhs.m_duration < rhs.m_duration;
  }
  
  friend bool operator == (precise_duration const& lhs, precise_duration const& rhs)
  {
    return lhs.m_duration == rhs.m_duration;
  }
  

  friend ptime& operator+=(ptime & u, precise_duration const& v)
  {
    u += v.m_duration;
    return u;
  }

  friend ptime& operator-=(ptime & u, precise_duration const& v)
  {
    u -= v.m_duration;
    return u;
  }

   friend scalar_type ratio_(precise_duration const& lhs, precise_duration const& rhs)
   { return lhs.total_nanoseconds() / rhs.total_nanoseconds(); }


private:
  InternalDurationType m_duration;

      };


   time_duration abs(time_duration d); 

} } // boost::time namespaces




namespace ossimplugins
{
namespace time
{
boost::posix_time::ptime readFormattedDate(const std::string & dateStr, const std::string & format = "%Y-%m-%dT%H:%M:%S%F");
}
};

#endif

#endif // ossimTimeUtilities_h
