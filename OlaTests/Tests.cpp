#include "TestMacros.h"

using namespace ola;


TEST(Operators, Additive)
{
	EXPECT_EQ(OLA(-i test_additive), 0);
}

TEST(Operators, Multiplicative)
{
	EXPECT_EQ(OLA(-i test_multiplicative), 0);
}

TEST(Operators, Relational)
{
	EXPECT_EQ(OLA(-i test_relational), 0);
}

TEST(Operators, Shift)
{
	EXPECT_EQ(OLA(-i test_shift), 0);
}

TEST(Operators, Bit)
{
	EXPECT_EQ(OLA(-i test_bit), 0);
}

TEST(Operators, Logical)
{
	EXPECT_EQ(OLA(-i test_logical), 0);
}

TEST(Operators, PlusMinus)
{
	EXPECT_EQ(OLA(-i test_plusminus), 0);
}

TEST(Operators, Increment)
{
	EXPECT_EQ(OLA(-i test_increment), 0);
}

TEST(Operators, TernaryOperator)
{
	EXPECT_EQ(OLA(-i test_ternary), 0);
}

TEST(Operators, Sizeof)
{
	EXPECT_EQ(OLA(-i test_sizeof), 0);
}

TEST(Control, IfElse)
{
	EXPECT_EQ(OLA(-i test_ifelse), 0);
}

TEST(Control, Switch)
{
	EXPECT_EQ(OLA(-i test_switch), 0);
}

TEST(Control, Goto)
{
	EXPECT_EQ(OLA(-i test_goto), 0);
}

TEST(Iteration, For)
{
	EXPECT_EQ(OLA(-i test_for), 0);
}

TEST(Iteration, While)
{
	EXPECT_EQ(OLA(-i test_while), 0);
}

TEST(Iteration, DoWhile)
{
	EXPECT_EQ(OLA(-i test_dowhile), 0);
}

TEST(Declarations, Functions) 
{
	
}

TEST(Declarations, Variables)
{

}

TEST(Declarations, Import)
{

}

TEST(Declarations, Alias)
{
	EXPECT_EQ(OLA(-i test_alias), 0);
}

TEST(Declarations, Enum)
{
	EXPECT_EQ(OLA(-i test_enum), 0);
}

TEST(Declarations, Class)
{
	EXPECT_EQ(OLA(-i test_class), 0);
}

TEST(Declarations, Ref)
{
	EXPECT_EQ(OLA(-i test_ref), 0);
}

TEST(Declarations, Array)
{
	EXPECT_EQ(OLA(-i test_array), 0);
}

TEST(Function, Calls)
{
	EXPECT_EQ(OLA(-i test_functioncalls), 0);
}


TEST(Misc, Strings)
{
	EXPECT_EQ(OLA(-i test_string), 0);
}

TEST(Misc, ImplicitCasts)
{
	EXPECT_EQ(OLA(-i test_implicitcasts), 0);
}




