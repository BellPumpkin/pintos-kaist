#ifndef THREADS_FLAGS_H
#define THREADS_FLAGS_H

#define FLAG_MBS   (1<<1)
#define FLAG_TF    (1<<8)
#define FLAG_IF    (1<<9)
#define FLAG_DF    (1<<10)
#define FLAG_IOPL  (3<<12)
#define FLAG_AC    (1<<18)
#define FLAG_NT    (1<<14)

#endif /* threads/flags.h */


/*
#define FLAG_IF (1<<9)는 C 또는 C++ 프로그램에서 사용할 수 있는 매크로입니다.
이 매크로는 FLAG_IF라는 상수를 정의하며, 해당 상수의 값은 (1 << 9)입니다.

(1 << 9)는 비트 시프트(shift) 연산을 사용하여 1을 왼쪽으로 9 비트 이동시킨 값을 나타냅니다.
이는 이진수로 표현하면 1 뒤에 9개의 0이 추가된 형태입니다. 따라서 FLAG_IF의 값은 1을 9번 왼쪽으로 시프트한 512가 됩니다.

이러한 매크로를 사용하면 코드에서 특정한 비트를 가지는 플래그나 특정한 상수를 의미하는 이름을 사용할 때 코드의 가독성을 높일 수 있습니다.
예를 들어, flags & FLAG_IF는 플래그 레지스터의 IF 비트가 설정되어 있는지 여부를 검사하는데 사용될 수 있으며,
이렇게 하면 코드가 더 이해하기 쉬워집니다.
*/