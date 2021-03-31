void a() {
    /* Your code */
}

void a_special( char const * caller_name ) {
    printf( "a was called from %s\n", caller_name );
    a();
}

#define a() a_special(__func__)

void b() {
    a();
}

int main() {
    b();
}