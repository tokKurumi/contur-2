#include <iostream>
using namespace std;

class LAN
{
public:
	LAN() {}

	void sendData(int data)
	{
		cout << "LAN send: data = " << data << endl;
	}

private:
protected:
};