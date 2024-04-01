#include <map>
#include <string>
#include "TwoDimenConfigFile.h"

using std::string;

void test1()
{
	TwoDimenConfigFile confiFile("pric.csv");

	//confiFile.Show();

	unsigned int lineNum = confiFile.LineNumber();
	printf("line number: %u\n", lineNum);

	//昨收价
	double ret = confiFile.GetData(0, "code");
	printf("%.0lf\n", ret);

	ret = confiFile.GetData(1, "code");
	printf("%.3lf\n", ret);
	
	ret = confiFile.GetData(1, "pre_price");
	printf("%.3lf\n", ret);
	
	ret = confiFile.GetData(1, "open_price");
	printf("%.3lf\n", ret);

	ret = confiFile.GetData(2, "code");

	ret = confiFile.GetData(1, "open_pr");
}

int main()
{
	test1();

	return 0;
}
