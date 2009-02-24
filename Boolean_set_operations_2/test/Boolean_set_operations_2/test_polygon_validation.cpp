/*test file for polygon validation. Intended for testing the global functions defined at Gps_polygon_validation.h*/


#ifndef CGAL_BSO_RATIONAL_NT_H
#define CGAL_BSO_RATIONAL_NT_H

#include <CGAL/basic.h>

#ifdef CGAL_USE_GMP
  // GMP is installed. Use the GMP rational number-type.
  #include <CGAL/Gmpq.h>
  typedef CGAL::Gmpq                                    Number_type;
#else
  // GMP is not installed. Use CGAL's exact rational number-type.
  #include <CGAL/MP_Float.h>
  #include <CGAL/Quotient.h>
  typedef CGAL::Quotient<CGAL::MP_Float>                Number_type;
#endif
#endif



#include <CGAL/Cartesian.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Boolean_set_operations_2/Gps_polygon_validation.h> 
#include <iterator>
#include <string>
#include <sstream>
#include <iostream>

typedef CGAL::Cartesian<Number_type>               Kernel;
typedef CGAL::Gps_segment_traits_2<Kernel>       	Traits_2;
typedef Traits_2::Polygon_2                        Polygon_2;
typedef Traits_2::Polygon_with_holes_2             Polygon_with_holes_2;

/*test files:

1. val_test1.dat - invalid polygon with holes. The hole is relatively simple instead of strictly simple.
2. val_test2.dat - invalid polygon with holes. The hole overlaps the outer boundary (the intersection results in a polygon).
3. val_test3.dat - invalid polygon with holes. Two holes intersect (the intersection results in a polygon). 
4. val_test4.dat - invalid polygon with holes. Two holes intersect (one contains the other).
5. val_test5.dat -  invalid polygon with holes. Two holes share an edge. (non regularized intersection 
results in an edge).
6. val_test6.dat - invalid polygon with holes. A hole and the outer boundary share an edge. (non regularized intersection 
results in an edge).
7. val_test7.dat - invalid polygon with holes. The outer boundary is not relatively simple because a "crossover" occurs
    at an intersection
8. val_test8.dat - valid polygon with holes. Outer boundary is relatively simple.
9. val_test9.dat - valid polygon with holes. Outer Boundary and holes are pairwise disjoint except on vertices
*/


/*test an input file. isValid indicates the input polygon is valid. ErrorMsg is displayed if the validation result does't equal isValid */
bool testValidationForFile(const char* infilename, std::ofstream& outfile , bool isValid) {
   std::ifstream input_file (infilename);

  if (! input_file.is_open()) {
    std::cerr << "Failed to open the " << infilename <<std::endl;
    return (false);
  }
  // Read a polygon with holes from a file.
  Polygon_2               outerP;
  unsigned int            num_holes;

  input_file >> outerP;
  input_file >> num_holes;

  std::vector<Polygon_2>  holes (num_holes);
  unsigned int            k;

  for (k = 0; k < num_holes; k++)
    input_file >> holes[k];
  Polygon_with_holes_2    P (outerP, holes.begin(), holes.end());
  Traits_2 tr;  
  bool testValid = CGAL::is_valid_polygon_with_holes(P,tr);
  bool res = true;
  if (testValid != isValid) {
    res=false;
    outfile<< "Error validating " << infilename <<std::endl;
    //outfile << "P = " ;
    //print_polygon_with_holes (P);
    outfile<<std::endl;  
  }  	  
  input_file.close();
  return res;
}

int main (int argc, char **argv) {
  std::cout << "Note for readers:\n"
            << "This test checks that the various validation functions do detect\n"
            << "invalide polygons. It runs the validation functions on invalid\n"
            << "polygons. For that reason, the following warnings must be ignored.\n\n";
  std::string testfilePrefix = "data/validation/val_test";
  std::string testfileSuffix = ".dat";   
  const char* outputfilename = "data/validation/validation_test_output.txt"; 
  std::ofstream output_file (outputfilename);
  if (! output_file.is_open()) {
    std::cerr << "Failed to open the " << outputfilename <<std::endl;
    return (1);
  }
  
  int result = 0;  
  for (int i=1;i<10;i++) {
    std::stringstream strs;
    std::string si;
    strs << i;
    strs >> si;     
    std::string filename = testfilePrefix + si + testfileSuffix;
    const char *cfilename = filename.c_str();
    bool isValidPgn=false;
    if (i>7)
      isValidPgn=true;    
    bool res =  testValidationForFile(cfilename, output_file, isValidPgn);
    if (!res) {
        std::cout << "test " << i << " failed" << std::endl;
        result=1;
    }  
    else {
      std::cout <<"test " << i << " succeeded" << std::endl;      
    }
  }
  if (result == 0)
    std::cout <<"ALL TESTS SUCCEEDED!" << std::endl; 
  else
  {
    std::cout <<"SOME TESTS FAILED" << std::endl; 
    return 1;
  }
  return (0);
}

