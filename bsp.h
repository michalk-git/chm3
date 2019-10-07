
#ifndef bsp_h
#define bsp_h

namespace Core_Health {

class BSP {
public:
    enum { TICKS_PER_SEC = 100 };

    static void init(int argc, char **argv);
    static void displayPaused(uint8_t const paused);
    static void displayPhilStat(uint8_t const n, char_t const *stat);
    static void terminate(int16_t const result);

    static void randomSeed(uint32_t const seed); // random seed
    static uint32_t random(void);                // pseudo-random generator

    // for testing...
    static void wait4SW1(void);
    static void ledOn(void);
    static void ledOff(void);
};

} // namespace Core_Health

#endif // bsp_h
