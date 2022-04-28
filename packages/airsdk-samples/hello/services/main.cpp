#include <future>
#define ULOG_TAG ms_main
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);


#include "native/sample.h"

#include "singulair/test.h"

int main(int argc, char *argv[]){
    
    ULOGI("Service Started");
    ULOGI("SAMPLE Started");
    auto f1 = std::async(std::launch::async, [](){
        sample::sample();
});
    ULOGI("TEST Started");
    auto f2 = std::async(std::launch::async, [](){
        test::test();
});

    return 0;
}
