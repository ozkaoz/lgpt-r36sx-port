/*
 * PhysicalInput.h -- capa de input fisico del port (R36SX pad).
 *
 * F1 de REFACTOR_ROADMAP_ES.md. Punto de partida: ninguna vista consume
 * todavia esta capa; la tabla de PhysicalKey espeja EXACTAMENTE los bits de
 * EPBM_* (View.h:19-40) para que el adapter F1b convierta sin aritmetica.
 *
 *   EPBM_LEFT=1, EPBM_DOWN=2, EPBM_RIGHT=4, EPBM_UP=8, EPBM_L=16 (L1),
 *   EPBM_B=32, EPBM_A=64, EPBM_R=128 (R1), EPBM_START=256, EPBM_SELECT=512,
 *   EPBM_X=1024, EPBM_Y=2048, EPBM_L2=4096, EPBM_R2=8192.
 *
 * Convencion del port: L1 = hombro izquierdo (EPBM_L), R1 = hombro derecho
 * (EPBM_R). Los nombres de esta capa usan L1/R1 para no confundirse con
 * LEFT/RIGHT.
 */
#ifndef UI_INPUT_PHYSICAL_INPUT_H_
#define UI_INPUT_PHYSICAL_INPUT_H_

namespace UI {
namespace Input {

typedef unsigned short PadMask;

enum PhysicalKey {
    KEY_NONE    = 0x0000,
    KEY_LEFT    = 0x0001,  /* EPBM_LEFT  */
    KEY_DOWN    = 0x0002,  /* EPBM_DOWN  */
    KEY_RIGHT   = 0x0004,  /* EPBM_RIGHT */
    KEY_UP      = 0x0008,  /* EPBM_UP    */
    KEY_L1      = 0x0010,  /* EPBM_L     */
    KEY_B       = 0x0020,  /* EPBM_B     */
    KEY_A       = 0x0040,  /* EPBM_A     */
    KEY_R1      = 0x0080,  /* EPBM_R     */
    KEY_START   = 0x0100,  /* EPBM_START */
    KEY_SELECT  = 0x0200,  /* EPBM_SELECT*/
    KEY_X       = 0x0400,  /* EPBM_X     */
    KEY_Y       = 0x0800,  /* EPBM_Y     */
    KEY_L2      = 0x1000,  /* EPBM_L2    */
    KEY_R2      = 0x2000   /* EPBM_R2    */
};

}  /* namespace Input */
}  /* namespace UI */

#endif  /* UI_INPUT_PHYSICAL_INPUT_H_ */
