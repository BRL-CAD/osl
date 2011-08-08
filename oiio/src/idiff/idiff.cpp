/*
  Copyright 2008 Larry Gritz and the other authors and contributors.
  All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of the software's owners nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  (This is the Modified BSD License)
*/


#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <iterator>

#include <boost/scoped_array.hpp>

#include <OpenEXR/ImathColor.h>
using Imath::Color3f;
#include <OpenEXR/ImathFun.h>

#include "dassert.h"
#include "argparse.h"
#include "imageio.h"
#include "imagecache.h"
#include "imagebuf.h"

#ifdef __APPLE__
 using std::isinf;
 using std::isnan;
#endif


OIIO_NAMESPACE_USING


enum idiffErrors {
    ErrOK = 0,            ///< No errors, the images match exactly
    ErrWarn,              ///< Warning: the errors differ a little
    ErrFail,              ///< Failure: the errors differ a lot
    ErrDifferentSize,     ///< Images aren't even the same size
    ErrFile,              ///< Could not find or open input files, etc.
    ErrLast
};



static bool verbose = false;
static bool outdiffonly = false;
static std::string diffimage;
static float diffscale = 1.0;
static bool diffabs = false;
static float warnthresh = 1.0e-6;
static float warnpercent = 0;
static float hardwarn = std::numeric_limits<float>::max();
static float failthresh = 1.0e-6;
static float failpercent = 0;
static bool perceptual = false;
static float hardfail = std::numeric_limits<float>::max();
static std::vector<std::string> filenames;
//static bool comparemeta = false;
static bool compareall = false;



static int
parse_files (int argc, const char *argv[])
{
    for (int i = 0;  i < argc;  i++)
        filenames.push_back (argv[i]);
    return 0;
}



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("idiff -- compare two images\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  idiff [options] image1 image2",
                  "%*", parse_files, "",
                  "--help", &help, "Print help message",
                  "-v", &verbose, "Verbose status messages",
                  "-a", &compareall, "Compare all subimages/miplevels",
                  "<SEPARATOR>", "Thresholding and comparison options",
                  "-fail %g", &failthresh, "Failure threshold difference (0.000001)",
                  "-failpercent %g", &failpercent, "Allow this percentage of failures (0)",
                  "-hardfail %g", &hardfail, "Fail if any one pixel exceeds this error (infinity)",
                  "-warn %g", &warnthresh, "Warning threshold difference (0.00001)",
                  "-warnpercent %g", &warnpercent, "Allow this percentage of warnings (0)",
                  "-hardwarn %g", &hardwarn, "Warn if any one pixel exceeds this error (infinity)",
                  "-p", &perceptual, "Perform perceptual (rather than numeric) comparison",
                  "<SEPARATOR>", "Difference image options",
                  "-o %s", &diffimage, "Output difference image",
                  "-od", &outdiffonly, "Output image only if nonzero difference",
                  "-abs", &diffabs, "Output image of absolute value, not signed difference",
                  "-scale %g", &diffscale, "Scale the output image by this factor",
//                  "-meta", &comparemeta, "Compare metadata",
                  NULL);
    if (ap.parse(argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }

    if (filenames.size() != 2) {
        std::cerr << "idiff: Must have two input filenames.\n";
        ap.usage();
        exit (EXIT_FAILURE);
    }
}



static bool
read_input (const std::string &filename, ImageBuf &img, 
            ImageCache *cache, int subimage=0, int miplevel=0)
{
    if (img.subimage() >= 0 && 
            img.subimage() == subimage && img.miplevel() == miplevel)
        return true;

    img.reset (filename, cache);
    if (img.read (subimage, miplevel, false, TypeDesc::TypeFloat))
        return true;

    std::cerr << "idiff ERROR: Could not read " << filename << ":\n\t"
              << img.geterror() << "\n";
    return false;
}



// Adobe RGB (1998) with reference white D65 -> XYZ
// matrix is from http://www.brucelindbloom.com/
inline Color3f
AdobeRGBToXYZ (const Color3f &rgb)
{
    return Color3f (rgb[0] * 0.576700f  + rgb[1] * 0.185556f  + rgb[2] * 0.188212f,
                    rgb[0] * 0.297361f  + rgb[1] * 0.627355f  + rgb[2] * 0.0752847f,
                    rgb[0] * 0.0270328f + rgb[1] * 0.0706879f + rgb[2] * 0.991248f);
}



template<class T>
Imath::Vec3<T>
powf (const Imath::Vec3<T> &x, float y)
{
    return Imath::Vec3<T> (powf (x[0], y), powf (x[1], y), powf (x[2], y));
}



/// Convert a color in XYZ space to LAB space.
///
static Color3f
XYZToLAB (const Color3f xyz)
{
    // Reference white point
    static const Color3f white (0.576700f + 0.185556f + 0.188212f,
                                0.297361f + 0.627355f + 0.0752847f,
                                0.0270328f + 0.0706879f + 0.991248f);
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;

    Color3f r = xyz / white;
    Color3f f;
    for (int i = 0; i < 3; i++) {
        if (r[i] > epsilon)
            f[i] = powf (r[i], 1.0f / 3.0f);
        else
            f[i] = (kappa * r[i] + 16.0f) / 116.0f;
    }
    return Color3f (116.0f * f[1] - 16.0f,    // L
                    500.0f * (f[0] - f[1]),   // A
                    200.0f * (f[1] - f[2]));  // B
}



#define LAPLACIAN_MAX_LEVELS 8


class LaplacianPyramid
{
public:
    LaplacianPyramid (float *image, int _width, int _height) 
        : w(_width), h(_height)
    {
        level[0].insert (level[0].begin(), image, image+w*h);
        for (int i = 1;  i < LAPLACIAN_MAX_LEVELS;  ++i)
            convolve (level[i], level[i-1]);
    }

    ~LaplacianPyramid () { }

    float value (int x, int y, int lev) const {
	return level[std::min (lev, LAPLACIAN_MAX_LEVELS)][y*w + x];
    }

private:
    int w, h;
    std::vector<float> level[LAPLACIAN_MAX_LEVELS];

    // convolve image b with the kernel and store it in a
    void convolve (std::vector<float> &a, const std::vector<float> &b) {
        const float kernel[] = {0.05f, 0.25f, 0.4f, 0.25f, 0.05f};
        a.resize (b.size());
        for (int y = 0, index = 0;  y < h;  ++y) {
            for (int x = 0;  x < w;  ++x, ++index) {
                a[index] = 0.0f;
                for (int i = -2;  i <= 2;  ++i) {
                    for (int j = -2;  j<= 2;  ++j) {
                        int nx = abs(x+i);
                        int ny = abs(y+j);
                        if (nx >= w)
                            nx=2*w-nx-1;
                        if (ny >= h)
                            ny=2*h-ny-1;
                        a[index] += kernel[i+2] * kernel[j+2] * b[ny * w + nx];
                    } 
                }
            }
        }
    }
};



// Contrast sensitivity function (Barten SPIE 1989)
static float
contrast_sensitivity (float cyclesperdegree, float luminance)
{
    float a = 440.0f * powf ((1.0f + 0.7f / luminance), -0.2f);
    float b = 0.3f * powf ((1.0f + 100.0f / luminance), 0.15f);
    return a * cyclesperdegree * expf(-b * cyclesperdegree) 
             * sqrtf(1.0f + 0.06f * expf(b * cyclesperdegree)); 
}



// Visual Masking Function from Daly 1993
inline float
mask (float contrast)
{
    float a = powf (392.498f * contrast, 0.7f);
    float b = powf (0.0153f * a, 4.0f);
    return powf (1.0f + b, 0.25f); 
}



// Given the adaptation luminance, this function returns the
// threshold of visibility in cd per m^2
// TVI means Threshold vs Intensity function
// This version comes from Ward Larson Siggraph 1997
static float
tvi (float adaptation_luminance)
{
    // returns the threshold luminance given the adaptation luminance
    // units are candelas per meter squared
    float r;
    float log_a = log10f(adaptation_luminance);
    if (log_a < -3.94f)
        r = -2.86f;
    else if (log_a < -1.44f)
        r = powf(0.405f * log_a + 1.6f , 2.18f) - 2.86f;
    else if (log_a < -0.0184f)
        r = log_a - 0.395f;
    else if (log_a < 1.9f)
        r = powf(0.249f * log_a + 0.65f, 2.7f) - 0.72f;
    else
        r = log_a - 1.255f;
    return powf (10.0f, r); 
}



/// Use Hector Yee's perceptual metric.  Return the number of pixels that
/// fail the comparison.
/// N.B. - assume pixels are already in linear color space.
int
Yee_Compare (const ImageBuf &img0, const ImageBuf &img1,
             float luminance = 100, float fov = 45)
{
    const ImageSpec &spec (img0.spec());
    ASSERT (spec.format == TypeDesc::FLOAT);
    int nscanlines = spec.height * spec.depth;
    int npels = nscanlines * spec.width;

    bool luminanceOnly = false;

    // assuming colorspaces are in Adobe RGB (1998), convert to LAB
    boost::scoped_array<Color3f> aLAB (new Color3f[npels]);
    boost::scoped_array<Color3f> bLAB (new Color3f[npels]);
    boost::scoped_array<float> aLum (new float[npels]);
    boost::scoped_array<float> bLum (new float[npels]);
    ImageBuf::ConstIterator<float,float> pix0 (img0);
    ImageBuf::ConstIterator<float,float> pix1 (img1);
    for (int i = 0;  pix0.valid();  ++i, pix0++) {
        pix1.pos (pix0.x(), pix0.y());  // ensure alignment
        Color3f RGB, XYZ;
        RGB.setValue (pix0[0], pix0[1], pix0[2]);
        XYZ = AdobeRGBToXYZ (RGB);
        aLAB[i] = XYZToLAB (XYZ);
        aLum[i] = XYZ[1] * luminance;

        RGB.setValue (pix1[0], pix1[1], pix1[2]);
        XYZ = AdobeRGBToXYZ (RGB);
        bLAB[i] = XYZToLAB (XYZ);
        bLum[i] = XYZ[1] * luminance;
    }

    // Construct Laplacian pyramids
    LaplacianPyramid la (&aLum[0], spec.width, nscanlines);
    LaplacianPyramid lb (&bLum[0], spec.width, nscanlines);

    float num_one_degree_pixels = (float) (2 * tan(fov * 0.5 * M_PI / 180) * 180 / M_PI);
    float pixels_per_degree = spec.width / num_one_degree_pixels;

    unsigned int adaptation_level = 0;
    for (int i = 0, npixels = 1;
             i < LAPLACIAN_MAX_LEVELS && npixels <= num_one_degree_pixels;
             ++i, npixels *= 2) 
        adaptation_level = i;

    float cpd[LAPLACIAN_MAX_LEVELS];
    cpd[0] = 0.5f * pixels_per_degree;
    for (int i = 1;  i < LAPLACIAN_MAX_LEVELS;  ++i)
        cpd[i] = 0.5f * cpd[i - 1];
    float csf_max = contrast_sensitivity (3.248f, 100.0f);

    float F_freq[LAPLACIAN_MAX_LEVELS - 2];
    for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2;  ++i)
        F_freq[i] = csf_max / contrast_sensitivity (cpd[i], 100.0f);

    unsigned int pixels_failed = 0;
    for (int y = 0, index = 0; y < nscanlines;  ++y) {
        for (int x = 0;  x < spec.width;  ++x, ++index) {
            float contrast[LAPLACIAN_MAX_LEVELS - 2];
            float sum_contrast = 0;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++) {
                float n1 = fabsf (la.value(x,y,i) - la.value(x,y,i+1));
                float n2 = fabsf (lb.value(x,y,i) - lb.value(x,y,i+1));
                float numerator = std::max (n1, n2);
                float d1 = fabsf (la.value(x,y,i+2));
                float d2 = fabsf (lb.value(x,y,i+2));
                float denominator = std::max (std::max (d1, d2), 1.0e-5f);
                contrast[i] = numerator / denominator;
                sum_contrast += contrast[i];
            }
            if (sum_contrast < 1e-5)
                sum_contrast = 1e-5f;
            float F_mask[LAPLACIAN_MAX_LEVELS - 2];
            float adapt = la.value(x,y,adaptation_level) + lb.value(x,y,adaptation_level);
            adapt *= 0.5f;
            if (adapt < 1e-5)
                adapt = 1e-5f;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++)
                F_mask[i] = mask(contrast[i] * contrast_sensitivity(cpd[i], adapt)); 
            float factor = 0;
            for (int i = 0; i < LAPLACIAN_MAX_LEVELS - 2; i++)
                factor += contrast[i] * F_freq[i] * F_mask[i] / sum_contrast;
            factor = Imath::clamp (factor, 1.0f, 10.0f);
            float delta = fabsf (la.value(x,y,0) - lb.value(x,y,0));
            bool pass = true;
            // pure luminance test
            if (delta > factor * tvi(adapt)) {
                pass = false;
            } else if (! luminanceOnly) {
                // CIE delta E test with modifications
                float color_scale = 1.0f;
                // ramp down the color test in scotopic regions
                if (adapt < 10.0f) {
                    color_scale = 1.0f - (10.0f - color_scale) / 10.0f;
                    color_scale = color_scale * color_scale;
                }
                float da = aLAB[index][1] - bLAB[index][1];  // diff in A
                float db = aLAB[index][2] - bLAB[index][2];  // diff in B
                da = da * da;
                db = db * db;
                float delta_e = (da + db) * color_scale;
                if (delta_e > factor)
                    pass = false;
            }
            if (!pass)
                ++pixels_failed;
        }
    }
//    std::cout << "Perceptual diff shows " << pixels_failed << " failures\n";

    return pixels_failed;
}



static bool
same_size (const ImageBuf &A, const ImageBuf &B)
{
    const ImageSpec &a (A.spec()), &b (B.spec());
    return (a.width == b.width && a.height == b.height &&
            a.depth == b.depth && a.nchannels == b.nchannels);
}



static void
compare_images (const ImageBuf &A, const ImageBuf &B,
                double &meanerror, double &rms_error, double &PSNR,
                double &maxerror,
                int &maxx, int &maxy, int &maxz, int &maxc,
                int &nwarn, int &nfail)
{
    int npels = A.spec().width * A.spec().height * A.spec().depth;
    int nvals = npels * A.spec().nchannels;

    // Compare the two images.
    //
    double totalerror = 0;
    double totalsqrerror = 0;
    maxerror = 0;
    maxx=0, maxy=0, maxz=0, maxc=0;
    nfail = 0, nwarn = 0;
    float maxval = 1.0;  // max possible value
    ASSERT (A.spec().format == TypeDesc::FLOAT &&
            B.spec().format == TypeDesc::FLOAT);
    ImageBuf::ConstIterator<float,float> a (A);
    ImageBuf::ConstIterator<float,float> b (B);
    // Break up into batches to reduce cancelation errors as the error
    // sums become too much larger than the error for individual pixels.
    const int batchsize = 4096;   // As good a guess as any
    for ( ;  a.valid();  ) {
        double batcherror = 0;
        double batch_sqrerror = 0;
        for (int i = 0;  i < batchsize && a.valid();  ++i, ++a) {
            b.pos (a.x(), a.y());  // ensure alignment
            bool warned = false, failed = false;  // For this pixel
            for (int c = 0;  c < A.spec().nchannels;  ++c) {
                float aval = a[c], bval = b[c];
                maxval = std::max (maxval, std::max (aval, bval));
                double f = fabs (aval - bval);
                batcherror += f;
                batch_sqrerror += f*f;
                if (f > maxerror) {
                    maxerror = f;
                    maxx = a.x();
                    maxy = a.y();
                    maxz = 0;  // FIXME -- doesn't work for volume images
                    maxc = c;
                }
                if (! warned && f > warnthresh) {
                    ++nwarn;
                    warned = true;
                }
                if (! failed && f > failthresh) {
                    ++nfail;
                    failed = true;
                }
            }
        }
        totalerror += batcherror;
        totalsqrerror += batch_sqrerror;
    }
    meanerror = totalerror / nvals;
    rms_error = sqrt (totalsqrerror / nvals);
    PSNR = 20.0 * log10 (maxval / rms_error);
}



// function that standarize printing NaN and Inf values on
// Windows (where they are in 1.#INF, 1.#NAN format) and all
// others platform
inline void
safe_double_print (double val)
{
    if (isnan (val))
        std::cout << "nan";
    else if (isinf (val))
        std::cout << "inf";
    else
        std::cout << val;
    std::cout << '\n';
}



int
main (int argc, char *argv[])
{
    getargs (argc, argv);

    std::cout << "Comparing \"" << filenames[0] 
             << "\" and \"" << filenames[1] << "\"\n";

    // Create a private ImageCache so we can customize its cache size
    // and instruct it store everything internally as floats.
    ImageCache *imagecache = ImageCache::create (true);
    imagecache->attribute ("forcefloat", 1);
    if (sizeof(void *) == 4)  // 32 bit or 64?
        imagecache->attribute ("max_memory_MB", 512.0);
    else
        imagecache->attribute ("max_memory_MB", 2048.0);
    imagecache->attribute ("autotile", 256);
#ifdef DEBUG
    imagecache->attribute ("statistics:level", 2);
#endif

    ImageBuf img0, img1;
    if (! read_input (filenames[0], img0, imagecache) ||
        ! read_input (filenames[1], img1, imagecache))
        return ErrFile;
//    ImageSpec spec0 = img0.spec();  // stash it

    int ret = ErrOK;
    for (int subimage = 0;  subimage < img0.nsubimages();  ++subimage) {
        if (subimage > 0 && !compareall)
            break;
        if (subimage >= img1.nsubimages())
            break;

        if (compareall) {
            std::cout << "Subimage " << subimage << ": ";
            std::cout << img0.spec().width << " x " << img0.spec().height;
            if (img0.spec().depth > 1)
                std::cout << " x " << img0.spec().depth;
            std::cout << ", " << img0.spec().nchannels << " channel\n";
        }

        if (! read_input (filenames[0], img0, imagecache, subimage) ||
            ! read_input (filenames[1], img1, imagecache, subimage))
            return ErrFile;

        if (img0.nmiplevels() != img1.nmiplevels()) {
            std::cout << "Files do not match in their number of MIPmap levels\n";
        }

        for (int m = 0;  m < img0.nmiplevels();  ++m) {
            if (m > 0 && !compareall)
                break;
            if (m > 0 && img0.nmiplevels() != img1.nmiplevels()) {
                std::cout << "Files do not match in their number of MIPmap levels\n";
                ret = ErrDifferentSize;
                break;
            }

            if (! read_input (filenames[0], img0, imagecache, subimage, m) ||
                ! read_input (filenames[1], img1, imagecache, subimage, m))
                return ErrFile;

            if (compareall && img0.nmiplevels() > 1) {
                std::cout << " MIP level " << m << ": ";
                std::cout << img0.spec().width << " x " << img0.spec().height;
                if (img0.spec().depth > 1)
                    std::cout << " x " << img0.spec().depth;
                std::cout << ", " << img0.spec().nchannels << " channel\n";
            }

            // Compare the dimensions of the images.  Fail if they
            // aren't the same resolution and number of channels.  No
            // problem, though, if they aren't the same data type.
            if (! same_size (img0, img1)) {
                std::cout << "Images do not match in size: ";
                std::cout << "(" << img0.spec().width << "x" << img0.spec().height;
                if (img0.spec().depth > 1)
                    std::cout << "x" << img0.spec().depth;
                std::cout << "x" << img0.spec().nchannels << ")";
                std::cout << " versus ";
                std::cout << "(" << img1.spec().width << "x" << img1.spec().height;
                if (img1.spec().depth > 1)
                    std::cout << "x" << img1.spec().depth;
                std::cout << "x" << img1.spec().nchannels << ")\n";
                ret = ErrDifferentSize;
                break;
            }

            int npels = img0.spec().width * img0.spec().height * img0.spec().depth;
            ASSERT (img0.spec().format == TypeDesc::FLOAT);

            // Compare the two images.
            //
            double meanerror = 0;
            double maxerror = 0;
            double rms_error = 0;
            double PSNR = 0;
            int maxx=0, maxy=0, maxz=0, maxc=0;
            int nfail = 0, nwarn = 0;
            compare_images (img0, img1, meanerror, rms_error, PSNR, maxerror, 
                            maxx, maxy, maxz, maxc, nwarn, nfail);

            int yee_failures = 0;
            if (perceptual)
                yee_failures = Yee_Compare (img0, img1);

            // Print the report
            //
            std::cout << "  Mean error = ";
            safe_double_print (meanerror);
            std::cout << "  RMS error = ";
            safe_double_print (rms_error);
            std::cout << "  Peak SNR = ";
            safe_double_print (PSNR);
            std::cout << "  Max error  = " << maxerror;
            if (maxerror != 0) {
                std::cout << " @ (" << maxx << ", " << maxy;
                if (img0.spec().depth > 1)
                    std::cout << ", " << maxz;
                std::cout << ", " << img0.spec().channelnames[maxc] << ')';
            }
            std::cout << "\n";
// when Visual Studio is used float values in scientific foramt are 
// printed with three digit exponent. We change this behaviour to fit
// Linux way
#ifdef _MSC_VER
            _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
            int precis = std::cout.precision();
            std::cout << "  " << nwarn << " pixels (" 
                      << std::setprecision(3) << (100.0*nwarn / npels) 
                      << std::setprecision(precis) << "%) over " << warnthresh << "\n";
            std::cout << "  " << nfail << " pixels (" 
                      << std::setprecision(3) << (100.0*nfail / npels) 
                      << std::setprecision(precis) << "%) over " << failthresh << "\n";
            if (perceptual)
                std::cout << "  " << yee_failures << " pixels ("
                          << std::setprecision(3) << (100.0*yee_failures / npels) 
                          << std::setprecision(precis)
                          << "%) failed the perceptual test\n";

            if (nfail > (failpercent/100.0 * npels) || maxerror > hardfail ||
                yee_failures > (failpercent/100.0 * npels)) {
                ret = ErrFail;
            } else if (nwarn > (warnpercent/100.0 * npels) || maxerror > hardwarn) {
                if (ret != ErrFail)
                    ret = ErrWarn;
            }

            // If the user requested that a difference image be output,
            // do that.  N.B. we only do this for the first subimage
            // right now, because ImageBuf doesn't really know how to
            // write subimages.
            if (diffimage.size() && (maxerror != 0 || !outdiffonly)) {
                ImageBuf diff (diffimage, img0.spec());
                ImageBuf::ConstIterator<float,float> pix0 (img0);
                ImageBuf::ConstIterator<float,float> pix1 (img1);
                ImageBuf::Iterator<float,float> pixdiff (diff);
                // Subtract the second image from the first.  At which
                // time we no longer need the second image, so free it.
                if (diffabs) {
                    for (  ;  pix0.valid();  ++pix0) {
                        pix1.pos (pix0.x(), pix0.y());  // ensure alignment
                        pixdiff.pos (pix0.x(), pix0.y());
                        for (int c = 0;  c < img0.nchannels();  ++c)
                            pixdiff[c] = diffscale * fabsf (pix0[c] - pix1[c]);
                    }
                } else {
                    for (  ;  pix0.valid();  ++pix0) {
                        pix1.pos (pix0.x(), pix0.y());  // ensure alignment
                        pixdiff.pos (pix0.x(), pix0.y());
                        for (int c = 0;  c < img0.spec().nchannels;  ++c)
                            pixdiff[c] = diffscale * (pix0[c] - pix1[c]);
                    }
                }

                diff.save (diffimage);

                // Clear diff image name so we only save the first
                // non-matching subimage.
                diffimage = "";
            }
        }
    }

    if (compareall && img0.nsubimages() != img1.nsubimages()) {
        std::cout << "Images had differing numbers of subimages ("
                  << img0.nsubimages() << " vs " << img1.nsubimages() << ")\n";
        ret = ErrFail;
    }
    if (!compareall && (img0.nsubimages() > 1 || img1.nsubimages() > 1)) {
        std::cout << "Only compared the first subimage (of "
                  << img0.nsubimages() << " and " << img1.nsubimages() 
                  << ", respectively)\n";
    }

    if (ret == ErrOK)
        std::cout << "PASS\n";
    else if (ret == ErrWarn)
        std::cout << "WARNING\n";
    else
        std::cout << "FAILURE\n";

    ImageCache::destroy (imagecache);
    return ret;
}
