
#include <stdio.h>
#include <upo_rrt_planners/RandomNumbers.h>
#include <boost/random/lagged_fibonacci.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/once.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/math/constants/constants.hpp>

/// @cond IGNORE
namespace
{
    /// We use a different random number generator for the seeds of the
    /// other random generators. The root seed is from the number of
    /// nano-seconds in the current time, or given by the user.
    class RNGSeedGenerator
    {
    public:
        RNGSeedGenerator() :
            someSeedsGenerated_(false),
            firstSeed_((boost::uint32_t)(boost::posix_time::microsec_clock::universal_time() -
                boost::posix_time::ptime(boost::date_time::min_date_time)).total_microseconds()),
            sGen_(firstSeed_),
            sDist_(1, 1000000000),
            s_(sGen_, sDist_)
        {
        }

        boost::uint32_t firstSeed()
        {
            boost::mutex::scoped_lock slock(rngMutex_);
            return firstSeed_;
        }

        void setSeed(boost::uint32_t seed)
        {
            boost::mutex::scoped_lock slock(rngMutex_);
            if (seed > 0)
            {
                if (someSeedsGenerated_)
                {
                    printf("ERROR. Random number generation already started. Changing seed now will not lead to deterministic sampling.");
                }
                else
                {
                    // In this case, since no seeds have been generated yet, so we remember this seed as the first one.
                    firstSeed_ = seed;
                }
            }
            else
            {
                if (someSeedsGenerated_)
                {
                    printf("WARN. Random generator seed cannot be 0. Ignoring seed.");
                    return;
                }
                else
                {
                    printf("WARN. Random generator seed cannot be 0. Using 1 instead.");
                    seed = 1;
                }
            }
            sGen_.seed(seed);
        }

        boost::uint32_t nextSeed()
        {
            boost::mutex::scoped_lock slock(rngMutex_);
            someSeedsGenerated_ = true;
            return s_();
        }

    private:
        bool                       someSeedsGenerated_;
        boost::uint32_t            firstSeed_;
        boost::mutex               rngMutex_;
        boost::lagged_fibonacci607 sGen_;
        boost::uniform_int<>       sDist_;
        boost::variate_generator<boost::lagged_fibonacci607&, boost::uniform_int<> > s_;
    };

    static boost::once_flag g_once = BOOST_ONCE_INIT;
    static boost::scoped_ptr<RNGSeedGenerator> g_RNGSeedGenerator;

    void initRNGSeedGenerator()
    {
        g_RNGSeedGenerator.reset(new RNGSeedGenerator());
    }

    RNGSeedGenerator& getRNGSeedGenerator()
    {
        boost::call_once(&initRNGSeedGenerator, g_once);
        return *g_RNGSeedGenerator;
    }
}  // namespace
/// @endcond

boost::uint32_t RNG::getSeed()
{
    return getRNGSeedGenerator().firstSeed();
}

void RNG::setSeed(boost::uint32_t seed)
{
    getRNGSeedGenerator().setSeed(seed);
}

RNG::RNG() :
    generator_(getRNGSeedGenerator().nextSeed()),
    uniDist_(0, 1),
    normalDist_(0, 1),
    uni_(generator_, uniDist_),
    normal_(generator_, normalDist_)
{
}

double RNG::halfNormalReal(double r_min, double r_max, double focus)
{
    assert(r_min <= r_max);

    const double mean = r_max - r_min;
    double       v    = gaussian(mean, mean/focus);

    if (v > mean) v = 2.0 * mean - v;
    double r = v >= 0.0 ? v + r_min : r_min;
    return r > r_max ? r_max : r;
}

int RNG::halfNormalInt(int r_min, int r_max, double focus)
{
    int r = (int)floor(halfNormalReal((double)r_min, (double)(r_max) + 1.0, focus));
    return (r > r_max) ? r_max : r;
}

// From: "Uniform Random Rotations", Ken Shoemake, Graphics Gems III,
//       pg. 124-132
void RNG::quaternion(double value[4])
{
    double x0 = uni_();
    double r1 = sqrt(1.0 - x0), r2 = sqrt(x0);
    double t1 = 2.0 * boost::math::constants::pi<double>() * uni_(), t2 = 2.0 * boost::math::constants::pi<double>() * uni_();
    double c1 = cos(t1), s1 = sin(t1);
    double c2 = cos(t2), s2 = sin(t2);
    value[0] = s1 * r1;
    value[1] = c1 * r1;
    value[2] = s2 * r2;
    value[3] = c2 * r2;
}

// From Effective Sampling and Distance Metrics for 3D Rigid Body Path Planning, by James Kuffner, ICRA 2004
void RNG::eulerRPY(double value[3])
{
    value[0] = boost::math::constants::pi<double>() * (-2.0 * uni_() + 1.0);
    value[1] = acos(1.0 - 2.0 * uni_()) - boost::math::constants::pi<double>() / 2.0;
    value[2] = boost::math::constants::pi<double>() * (-2.0 * uni_() + 1.0);
}
