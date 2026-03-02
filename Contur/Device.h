#include <iostream>
using namespace std;

class Device
{
public:
	Device() {}
	void printData(int data)
	{
		cout << "Device printData: data = " << data << endl;
	}
private:
protected:
};