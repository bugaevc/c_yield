#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gen.h"

// simple fibonacci generator
gen_t *fib(int n)
{
    gen_t *handle = gen_init();
    for (int a = 0, b = 1; a < n;) {
        printf("In fib(%d), a = %d, b = %d\n", n, a, b);
        gen_yield(handle, (void *) a);
        int old_a = a;
        a = b;
        b += old_a;
    }
    gen_return(handle, NULL);
    return handle;
}

// test for correctly preserving arguments when copying the stack frame
gen_t* multiple_args(int a, int b, int c, int d, int e, int f)
{
    gen_t *handle = gen_init();
    gen_yield(handle, (void *) a);
    gen_yield(handle, (void *) b);
    gen_yield(handle, (void *) c);
    gen_yield(handle, (void *) d);
    gen_yield(handle, (void *) e);
    gen_yield(handle, (void *) f);
    gen_return(handle, NULL);
    return handle;
}

// interaction: gets strings, yields their lengths, returns the sum
gen_t *sum_str_lens(void)
{
    gen_t *handle = gen_init();
    size_t sum = 0;
    for (const char *s = ""; s;) {
        size_t len = strlen(s);
        sum += len;
        s = (const char *) gen_yield(handle, (void *) len);
    }
    gen_return(handle, (void *) sum);
    return handle;
}

// recursive fibonacci generator
gen_t *fib_awesome(void)
{
    // Unlike in Python, we have no GC, so we can't just drop the generator
    // and expect it to be cleaned up. Workaround: pass need_more, clean up
    // and return if set to false.
    gen_t *handle = gen_init();
    // we yield the first value unconditionally
    int need_more = (int) gen_yield(handle, (void *) 1);
    if (!need_more) {
        gen_return(handle, NULL);
        return handle;
    }
    gen_t *f1 = fib_awesome(), *f2 = fib_awesome();
    need_more = (int) gen_yield(handle, gen_send(f1, NULL));
    if (!need_more) {
        gen_send(f2, NULL); // first call
    }
    while (need_more) {
        int v1 = (int) gen_send(f1, (void *) 1);
        int v2 = (int) gen_send(f2, (void *) 1);
        need_more = (int) gen_yield(handle, (void *) (v1 + v2));
    }
    gen_send(f1, (void *) 0);
    assert(gen_is_done(f1));
    free(f1);

    gen_send(f2, (void *) 0);
    assert(gen_is_done(f2));
    free(f2);

    gen_return(handle, NULL);
    return handle;
}

// recursive, but not a generator! gets passed the handle in the first argument
void binaries_inner(gen_t *handle, char *start, char *c)
{
    if (*c == 0) {
        gen_yield(handle, (void *) start);
        return;
    }
    *c = '0';
    binaries_inner(handle, start, c + 1);
    *c = '1';
    binaries_inner(handle, start, c + 1);
}

// generate some binary numbers, calling gen_yield() deeper in the call stack.
// coroutine-like in that it basically "yeilds from" other function
gen_t *binaries(size_t len)
{
    gen_t *handle = gen_init();
    char buffer[len + 1];
    memset(buffer, 1, len);
    buffer[len] = 0;
    binaries_inner(handle, buffer, buffer);
    gen_return(handle, NULL);
    return handle;
}

// recursively generate natural numbers, passing handles back *to the caller*.
// coroutine-like in that runs on an event loop, yielding things to wait on.
// also, mutates its arguments
gen_t *numbers(size_t len, unsigned int base)
{
    gen_t *handle = gen_init();
    if (len == 0) {
        gen_return(handle, (void *) base);
        return handle;
    }
    len--;
    base *= 2;
    gen_t *h1 = numbers(len, base);
    gen_yield(handle, (void *) h1);

    base++;
    gen_t *h2 = numbers(len, base);
    gen_yield(handle, (void *) h2);

    gen_return(handle, (void *) -1);
    return handle;
}

// a pair of basic generators to demonstrate passing handles between stacks
gen_t *blind_ping_pong_inner(gen_t *parent, int times) {
    gen_t *handle = gen_init();
    for (int i = 0; i < times; i++) {
        gen_yield(parent, (void *) i); // yielding `i` from the *parent*
        gen_yield(handle, (void *) i); // yielding to the parent to be processed
    }
    gen_return(handle, NULL);
    return handle;
}

gen_t *blind_ping_pong(int times) {
    gen_t *handle = gen_init();
    gen_t *child = blind_ping_pong_inner(handle, times);
    while (1) {
        int num = (int) gen_send(child, NULL);
        if (gen_is_done(child)) {
            free(child);
            gen_return(handle, NULL);
            return handle;
        }
        num += 1000;
        gen_yield(handle, (void *) num);
    }
}

int main(void)
{
    gen_t *gen;

    printf("================ fib ====================\n");
    gen = fib(420);
    while (1) {
        int num = (int) gen_send(gen, NULL);
        if (gen_is_done(gen)) {
            break;
        }
        printf("Got %d\n", num);
    }
    free(gen);

    printf("================ multiple_args ==========\n");
    gen = multiple_args(1111, 2222, 3333, 4444, 5555, 6666);
    while (1) {
        int num = (int) gen_send(gen, NULL);
        if (gen_is_done(gen)) {
            break;
        }
        printf("Got %d\n", num);
    }
    free(gen);

    printf("================ sum_str_lens ===========\n");
    gen = sum_str_lens();
    // first call -- no value passed, it yields 0
    gen_send(gen, NULL);
    size_t len;
    printf("Now input some stuff:\n");
    for (char s[100]; scanf("%s", s) == 1;) {
        len = (size_t) gen_send(gen, s);
        printf("Length: %d\n", len);
    }
    // last yielded value is the return value
    len = (size_t) gen_send(gen, NULL);
    assert(gen_is_done(gen));
    printf("Total length: %d\n", len);
    free(gen);

    printf("================ fib_awesome ===========\n");
    gen = fib_awesome();
    for (int i = 0; i < 5; i++) {
        int f = (int) gen_send(gen, (void *) 1); // need more!
        printf("%d\n", f);
    }
    gen_send(gen, (void *) 0);
    assert(gen_is_done(gen));
    free(gen);

    printf("================ binaries ===============\n");
    gen = binaries(4);
    while (!gen_is_done(gen)) {
        const char *s = (const char *) gen_send(gen, NULL);
        if (s) {
            printf("Got %s\n", s);
        }
    }
    free(gen);

    printf("================ numbers ================\n");
    gen_t *stack[30];
    int ind = 0;
    stack[0] = numbers(4, 0);
    // super-basic event loop
    while (ind >= 0) {
        void *res = gen_yield(stack[ind], NULL);
        if (gen_is_done(stack[ind])) {
            int num = (int) res;
            if (num != -1) {
                printf("Got %d\n", num);
            }
            free(stack[ind--]);
        } else {
            stack[++ind] = res;
        }
    }

    printf("================ blind_ping_pong ========\n");
    gen = blind_ping_pong(5);
    while (1) {
        int num = (int) gen_send(gen, NULL);
        if (gen_is_done(gen)) {
            break;
        }
        printf("Got %d\n", num);
    }
    free(gen);

    return 0;
}
