/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <cstdint>
#include <libdragon.h>

namespace MIPS
{
  namespace REG
  {
    constexpr uint32_t ZERO = 0;
    constexpr uint32_t AT = 1;
    constexpr uint32_t V0 = 2;
    constexpr uint32_t V1 = 3;
    constexpr uint32_t A0 = 4;
    constexpr uint32_t A1 = 5;
    constexpr uint32_t A2 = 6;
    constexpr uint32_t A3 = 7;
  }

  constexpr uint32_t I_TYPE(uint32_t opcode, uint32_t regDst, uint32_t regSrc, uint16_t val) {
    return (opcode << 26) | (regSrc << 21) | (regDst << 16) | val;
  }
  constexpr uint32_t J_TYPE(uint32_t opcode, uint32_t target) {
    return (opcode << 26) | (target & 0x03FF'FFFF);
  }

  constexpr uint32_t NOP() { return 0; }

  constexpr uint32_t LUI(uint32_t regDst, uint16_t val) {
    return I_TYPE(0b001111, regDst, REG::ZERO, val);
  }

  constexpr uint32_t ORI(uint32_t regDst, uint32_t regSrc, uint16_t val) {
    return I_TYPE(0b001101, regDst, regSrc, val);
  }

  constexpr uint32_t JUMP(void* address) {
    return J_TYPE(0b000010, ((uint32_t)((uintptr_t)address & 0x00FFFFFF)) >> 2);
  }
}