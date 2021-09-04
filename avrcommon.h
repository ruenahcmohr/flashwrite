#ifndef __avrcommon_h
  #define __avrcommon_h
  
  
  #define SetBit(BIT, PORT)    (PORT |= (1<<BIT))
  #define ClearBit(BIT, PORT)  (PORT &= ~(1<<BIT))
  #define IsHigh(BIT, PORT)    (PORT & (1<<BIT)) != 0
  #define IsLow(BIT, PORT)     (PORT & (1<<BIT)) == 0
  #define NOP()                 asm volatile ("nop"::)
  #define ABS(a)                ((a) < 0 ? -(a) : (a))
  #define SIGN(x)               (x)==0?0:(x)>0?1:-1
  #define limit(v, l, h)        ((v) > (h)) ? (h) : ((v) < (l)) ? (l) : (v)
  #define inBounds(v, l, h)        ((v) > (h)) ? (0) : ((v) < (l)) ? (0) : (1)
  
  // for linear remapping of number ranges, see arduino map()
  // think of a line, between Il,Ol and Ih,Oh, this solves the y for given x position
  #define RangeRemap(v,Il,Ih,Ol,Oh) (((((v)-(Il))*((Oh)-(Ol)))/((Ih)-(Il)))+(Ol))
    
  // for bounded ramping
    #define RampUp(k,top)  (((k)<(top))?(k++):(k+=0))
    #define RampDown(k,bot) (((k)>(bot))?(k--):(k+=0))
    
#endif
