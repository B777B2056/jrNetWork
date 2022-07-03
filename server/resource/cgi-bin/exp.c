#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {    
    char *pszMethod;
    pszMethod =getenv("REQUEST_METHOD");
    if(strcmp(pszMethod,"GET") == 0) {    
        //读取环境变量来获取数据
        printf("<html>");
	    printf("<head>");
	    printf("<meta charset=\"utf-8\">");
	    printf("<title>CGI Test</title>");
	    printf("</head>");
	    printf("<body>");
	    printf("<h2>Parameters:%s</h2>", getenv("QUERY_STRING"));
	    printf("</body>");
	    printf("</html>");
        fflush(stdout);
    } else {  
        //读取STDIN来获取数据
        int iLength=atoi(getenv("CONTENT_LENGTH"));
        printf("This is POSTMETHOD!\n");
        printf("input data is:\n");
        for(int i=0;i<iLength;i++) {
            char cGet;
            scanf("%c", &cGet);
            printf("%c", cGet);
        }
    }
    return 0;
}
