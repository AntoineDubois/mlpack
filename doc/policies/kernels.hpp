/*! @page kernels The KernelType policy in mlpack

`Kernel methods' make up a large class of machine learning techniques.  Each of
these methods is characterized by its dependence on a \b kernel \b function.  In
rough terms, a kernel function is a general notion of similarity between two
points, with its value large when objects are similar and its value small when
objects are dissimilar (note that this is not the only interpretation of what a
kernel is).

A kernel (or `Mercer kernel') \f$\mathcal{K}(\cdot, \cdot)\f$ takes two objects
as input and returns some sort of similarity value.  The specific details and
properties of kernels are outside the scope of this documentation; for a better
introduction to kernels and kernel methods, there are numerous better resources
available, including \ref
http://www.eric-kim.net/eric-kim-net/posts/1/kernel_trick.html "Eric Kim's
tutorial".

mlpack implements a number of kernel methods and, accordingly, each of these
methods allows arbitrary kernels to be used via the \c KernelType template
parameter.  Like the \ref metrics "MetricType policy", the requirements are
quite simple: a class implementing the \c KernelType policy must have

 - an \c Evaluate() function
 - a default constructor

The signature of the \c Evaluate() function is straightforward:

@code
template<typename VecTypeA, typename VecTypeB>
double Evaluate(const VecTypeA& a, const VecTypeB& b);
@endcode

The function takes two vector arguments, \c a and \c b, and returns a \c double
that is the evaluation of the kernel between the two arguments.  So, for a
particular kernel \f$\mathcal{K}(\cdot, \cdot)\f$, the \c Evaluate() function
should return \f$\mathcal{K}(a, b)\f$.

The arguments \c a and \c b, of types \c VecTypeA and \c VecTypeB, respectively,
will be an Armadillo-like vector type (usually \c arma::vec, \c arma::sp_vec, or
similar).  In general it should be valid to assume that \c VecTypeA is a class
with the same API as \c arma::vec.

Note that for kernels that do not hold any state, the \c Evaluate() method can
be marked as \c static.

Overall, the \c KernelType template policy is quite simple (much like the
\ref metrics "MetricType policy").  Below is an example kernel class, which
outputs \c 1 if the vectors are close and \c 0 otherwise.

@code
class ExampleKernel
{
  // Default constructor is required.
  ExampleKernel() { }

  // The example kernel holds no state, so we can mark Evaluate() as static.
  template<typename VecTypeA, typename VecTypeB>
  static double Evaluate(const VecTypeA& a, const VecTypeB& b)
  {
    // Get how far apart the vectors are (using the Euclidean distance).
    const double distance = arma::norm(a - b);

    if (distance < 0.05) // Less than 0.05 distance is "close".
      return 1;
    else
      return 0;
  }
};
@endcode

Then, this kernel may be easily used inside of mlpack algorithms.  For instance,
the code below runs kernel PCA (\c mlpack::kpca::KernelPCA) on a random dataset
using the \c ExampleKernel.  The results are saved to a file called
\c results.csv.  (Note that this is simply an example to demonstrate usage, and
this example kernel isn't actually likely to be useful in practice.)

@code
#include <mlpack/core.hpp>
#include <mlpack/methods/kernel_pca/kernel_pca.hpp>
#include "example_kernel.hpp" // Contains the ExampleKernel class.

using namespace mlpack;
using namespace mlpack::kpca;
using namespace arma;

int main()
{
  // Generate the random dataset; 10 dimensions, 5000 points.
  mat dataset = randu<mat>(10, 5000);

  // Instantiate the KernelPCA object with the ExampleKernel kernel type.
  KernelPCA<ExampleKernel> kpca;

  // The dataset will be transformed using kernel PCA with the example kernel to
  // contain only 2 dimensions.
  kpca.Apply(dataset, 2);

  // Save the results to 'results.csv'.
  data::Save(dataset, "results.csv");
}
@endcode

mlpack comes with a number of pre-written kernels that satisfy the \c KernelType
policy:

 - mlpack::kernel::LinearKernel
 - mlpack::kernel::GaussianKernel
 - mlpack::kernel::HyperbolicTangentKernel
 - mlpack::kernel::EpanechnikovKernel
 - mlpack::kernel::CosineDistance
 - mlpack::kernel::LaplacianKernel
 - mlpack::kernel::PolynomialKernel
 - mlpack::kernel::TriangularKernel
 - mlpack::kernel::SphericalKernel
 - mlpack::kernel::PSpectrumStringKernel -- operates on strings, not vectors

These kernels (or a custom kernel) may be used in a variety of mlpack methods:

 - mlpack::kpca::KernelPCA - kernel principal components analysis
 - mlpack::fastmks::FastMKS - fast max-kernel search
 - mlpack::kernel::NystroemMethod - the Nystroem method for sampling
 - mlpack::metric::IPMetric - a metric built on a kernel

*/
