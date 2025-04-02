#include <iostream>
#include <limits>
#include <typeinfo>
#include <cstdint>
#include <cmath>
#include <random>

// **** This is the SCALE macro and its helper functions! ****
// **** don't modify them, these must stay consistent. ****
// **** that's why they are super compressed! ****
#define SCALE(x, l1, h1, l2, h2) (((x) - (l1)) * ((h2) - (l2)) / ((h1) - (l1)) + (l2))
#define PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, typeText) \
    std::cout << "SCALE(" << x << ", " << l1 << ", " << h1 << ", " << l2 << ", " << h2 << ") " << typeText << " = " \
              << result << " | Type: " << typeid(result).name() << std::endl;
template <typename T, typename U, typename V, typename W, typename X> void TestScale(T x, U l1, V h1, W l2, X h2) { auto result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, ""); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsInt(T x, U l1, V h1, W l2, X h2) { int result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as int"); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsUnsignedInt(T x, U l1, V h1, W l2, X h2) { unsigned result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as unsigned int"); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsInt64(T x, U l1, V h1, W l2, X h2) { int64_t result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as int64_t"); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsUnsignedInt64(T x, U l1, V h1, W l2, X h2) { uint64_t result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as uint64_t"); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsFloat(T x, U l1, V h1, W l2, X h2) { float result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as float"); }
template <typename T, typename U, typename V, typename W, typename X> void TestScaleAsDouble(T x, U l1, V h1, W l2, X h2) { double result = SCALE(x, l1, h1, l2, h2); PRINT_SCALE_RESULT(x, l1, h1, l2, h2, result, "as double"); }
float GenerateRandomFloat(float min, float max) { static std::random_device rd; static std::mt19937 gen(rd()); return std::uniform_real_distribution<float>(min, max)(gen); }

// **** Helper macros, variables and functions. ****
// **** This section is safe to modify. ****
#define SCREEN_HEIGHT 1080
#define SCREEN_WIDTH 1920
 // Pretend the game has been running for some amount of time.
 // Substitute one of these for a RageTimer::GetTimeSinceStart or GetTimeSinceStartFast call.
 // If you want to generate a GetTimeSinceStartMicroseconds value from any of these,
 // you can multiply one of these runtime values by 1000000 and cast it to a uint64_t.
const double RUNTIME_75M = 4500.0, RUNTIME_40M = 2400.0, RUNTIME_20M = 1200.0;

// Generate a series of random numbers to perform tests with
const float random_float = GenerateRandomFloat(-100.0f, 100.0f);
//const double random_double = random_float * 24.0;
const int random_int = static_cast<int>(random_float) * 3;
const int64_t random_int64 = std::round(std::pow(static_cast<double>(random_int), std::abs(random_float / 7.0)));

/*
 * Actual testing takes place below.
 * There is some example code here too.
 */

int main()
{
	std::cout << "today's lucky numbers: " << random_int << ", " << random_int64 << ", " << random_float << std::endl;
	//TestScaleAsInt(random_float,0.f,1.f,0.f,-SCREEN_HEIGHT/2);
	//float test = int((((random_float)-(0.f)) * ((-SCREEN_HEIGHT / 2) - (0.f)) / ((1.f) - (0.f)) + (0.f)));
	//std::cout << "inlined:" << test << std::endl;

	//float simplified1 = static_cast<int>(random_float * (-SCREEN_HEIGHT / 2));
	//std::cout << "simplified " << simplified1 << std::endl;

	return 0;
}
