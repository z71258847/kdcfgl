#include <stdio.h>

int main(){
	int i=0, ans=0;
	__attribute__((annotate(""))) int key;
	key=7;
	for (i=0; i<8; i++){
		int c=(key>>i)&1;
		if (c){
			ans++;
		}
	}
}
