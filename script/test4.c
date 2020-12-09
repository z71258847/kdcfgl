#include <stdio.h>

int main(){
	int i=0, ans0=0, ans1=0, ans2=0, ans3=0;
	__attribute__((annotate(""))) int key;
	key=7;
	for (i=0; i<8; i++){
		int c=(key>>i)&1;
		if (c){
			if (i%2==0){
				ans0++;
			}
			else{
				ans1++;
			}
		}
		else{
			if (i%2==1){
				ans2++;
			}
			else{
				ans3++;
			}
		}
	}
	printf("%d %d %d %d\n", ans0, ans1, ans2, ans3);
}
