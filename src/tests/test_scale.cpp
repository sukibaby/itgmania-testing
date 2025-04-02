#include <iostream>
#include <limits>
#include <typeinfo>

#define SCALE(x, l1, h1, l2, h2) (((x) - (l1)) * ((h2) - (l2)) / ((h1) - (l1)) + (l2))

///*
// * @brief Scales x so that l1 corresponds to l2 and h1 corresponds to h2.
// *
// * This does not modify x, so it MUST assign the result to something!
// * Do the multiply before the divide to that integer scales have more precision.
// *
// * One such example: SCALE(x, 0, 1, L, H); interpolate between L and H.
// *
// * define SCALE(x, l1, h1, l2, h2)	(((x) - (l1)) * ((h2) - (l2)) / ((h1) - (l1)) + (l2)) 
// 
// Lines 47 to 55 in itgmania/src/RageUtil.h
//
// Test for what type SCALE tries to store data as with the TestScale function.
// Test for what value is actually being stored with the TestScaleAs___ function.
// static_casts are explicitly -not- used to preserve original behavior.
// 
// You may wish to define variables to test with, or use the numeric limits,
// as well as make use of the functions which store the result in different types
// to fully understand how the SCALE macro behaved in the original code.
// 
// It's also a good idea to test this on something like godbolt.org where
// you can compare the results of different compilers used for different OS's.
// 
// Available functions:
//  - TestScale (stores result in an auto variable)
//  - TestScaleAsInt (stores result in an int variable)
//  - TestScaleAsUnsignedInt (stores result in an unsigned int variable)
//  - TestScaleAsInt64 (stores result in an int64_t variable)
//  - TestScaleAsUnsignedInt64 (stores result in an uint64_t variable)
//  - TestScaleAsFloat (stores result in a float variable)
//  - TestScaleAsDouble (stores result in a double variable)

template <typename T, typename U, typename V, typename W, typename X>
void TestScale(T x, U l1, V h1, W l2, X h2)
{
	auto result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsInt(T x, U l1, V h1, W l2, X h2)
{
	int result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as int = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsUnsignedInt(T x, U l1, V h1, W l2, X h2)
{
	unsigned result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as unsigned int = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsInt64(T x, U l1, V h1, W l2, X h2)
{
	int64_t result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as int64_t = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsUnsignedInt64(T x, U l1, V h1, W l2, X h2)
{
	uint64_t result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as uint64_t = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsFloat(T x, U l1, V h1, W l2, X h2)
{
	float result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as float = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

template <typename T, typename U, typename V, typename W, typename X>
void TestScaleAsDouble(T x, U l1, V h1, W l2, X h2)
{
	double result = SCALE(x, l1, h1, l2, h2);
	std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") as double = "
		<< result << " | Type: " << typeid(result).name() << std::endl;
}

int main()
{
	std::cout << "- SCALE test -\n";

	//TestScale(std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max());
	//TestScale(std::numeric_limits<double>::min(), 0.0, 1.0, std::numeric_limits<double>::max(), 0.0);

	//TestScale(0, 0, 1, 0, 1);
	//TestScaleAsInt(0, 0, 1, 0, 1);
	//TestScaleAsUnsignedInt(0, 0, 1, 0, 1);
	//TestScaleAsInt64(0, 0, 1, 0, 1);
	//TestScaleAsUnsignedInt64(0, 0, 1, 0, 1);
	//TestScaleAsFloat(0, 0, 1, 0, 1);
	//TestScaleAsDouble(0, 0, 1, 0, 1);

	return 0;
}
