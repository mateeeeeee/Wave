#include "pch.h"
#include "TestMacros.h"

using namespace wave;

TEST(Operators, Additive)
{
	EXPECT_EQ(WAVE(-i test_additive.wv), 0);
}

TEST(Operators, Multiplicative)
{
	EXPECT_EQ(WAVE(-i test_multiplicative.wv), 0);
}

TEST(Operators, Relational)
{
	EXPECT_EQ(WAVE(-i test_relational.wv), 0);
}

TEST(Operators, Shift)
{
	EXPECT_EQ(WAVE(-i test_shift.wv), 0);
}

TEST(Operators, Bit)
{
	EXPECT_EQ(WAVE(-i test_bit.wv), 0);
}

TEST(Operators, Logical)
{
	EXPECT_EQ(WAVE(-i test_logical.wv), 0);
}

TEST(Operators, PlusMinus)
{
	EXPECT_EQ(WAVE(-i test_plusminus.wv), 0);
}

TEST(Operators, Increment)
{
	EXPECT_EQ(WAVE(-i test_increment.wv), 0);
}

TEST(Operators, TernaryOperator)
{
	EXPECT_EQ(WAVE(-i test_ternary.wv), 0);
}

TEST(Operators, Sizeof)
{
	EXPECT_EQ(WAVE(-i test_sizeof.wv), 0);
}

TEST(Control, IfElse)
{
	EXPECT_EQ(WAVE(-i test_ifelse.wv), 0);
}

TEST(Control, Switch)
{
	EXPECT_EQ(WAVE(-i test_switch.wv), 0);
}

TEST(Control, Goto)
{
	EXPECT_EQ(WAVE(-i test_goto.wv), 0);
}

TEST(Control, Break)
{

}

TEST(Control, Continue)
{

}

TEST(Iteration, For)
{
	EXPECT_EQ(WAVE(-i test_for.wv), 0);
}

TEST(Iteration, While)
{
	EXPECT_EQ(WAVE(-i test_while.wv), 0);
}

TEST(Iteration, DoWhile)
{
	EXPECT_EQ(WAVE(-i test_dowhile.wv), 0);
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

TEST(Functions, Return)
{

}

TEST(Functions, Calling)
{

}

TEST(Linkage, PublicPrivate)
{

}

TEST(Linkage, Extern)
{

}

TEST(Misc, Const)
{

}

TEST(Misc, Constexpr)
{

}

TEST(Misc, Enum)
{

}
