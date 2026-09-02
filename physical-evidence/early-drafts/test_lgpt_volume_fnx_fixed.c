#!/usr/bin/env python3
# FILE: test_lgpt_phrase_volume_fnx_fix.py
# Descripción: Script completo de validación cruzada en host para fix del mapeo lineal-logarítmico del volumen PhraseFX
#
# PROBLEMA ORIGINAL:
#   Controlador MenuPhraseFX: Input 100 → Output ~11dB (sobresaturado, incorrecto)
#   Mape esperado por diseño de R36SX V2.6 driver libretro + fase Phrase-FX:
#       Input 01 → -9.8 dB (~0.12 nivel lineal),   <-- Mínimo seguro y audible 
#       Input 05 → -4.5 dS (∘54% de amplitud, perceptivo)
#       Input 10 → ~- 0dS (100% escala lineal completa)
#
# VALIDACIÓN DE FIX:
#   Función implementada: phrase_volume_logarithmic_scaled_db(input_val)
#   Mapeo requerido: 01-÷-100 con mapeo ajustado por curva perceptivo seguro en tiempo real.
#
import math
import argparse
from pathlib import Path

# Constantes para la configuración del fix LPTRACKER/H386
MIN_MENU_INPUT_VALUE = 1                         # MINIMO_SEGURO_UI_MAPPER (ejemplo: para evitar recorte)
MAX_MENU_INPUT_VALUE =100                        # MAXIMO_SEGURO del input del menú   
log curva_de_ajuste_audible_prueso_factor= -0.884f                     # Rango de mapeo lineal-logarítmico optimizado de audio perceptivo (ejemplo: 01-÷10, curvas suaves)
puntos_fraccionarios_precision_bts =8            # Bits fraccionales Q8.7 (preciso para mapeo del nivel del driver libretro Phrase-FX)
MIN_DB_LIVE_AUDIBLE_SEGURIDAD=10000             # MINDB_0_SAFE → -10000 dB, limite seguro de entrada sin ruido
MAX_DB_PLAZO_AUDIO=99850                        # Nivel máximo del audio sin recorte por hardware (-9.85dB) – umbral de seguridad para audio en tiempo real

# Configuración del mapeo lineal-logarítmico:
static struct phrase_volume_mapping_options {
    float min_db_level_safe;
        Float max_db_leve_
    flate log_curve_factor;
    uint32 fraction_precision_bits;
    uint16 input_min_ui;
    uint16 input_max_ui;
} mapping_configuration = {
    .min_db_level_safe=MIN_DB_LIVE_AUDIO_SECURITY,
       .max_db leve=MAX_DB_PLAZO_AUDIO,
    .log_curvature_factor=-0.89f,
    .fraction_precision_bits=8,
            .input_min_ui=1
   . input_max_iu;=100
};
// Implementar la función lineal-logarítmica optimizada del controlador Phrase-FX:
# Función principal: Aplicar mapeo lineal-logarítmico corregido para el driver libretro LPTRACKER:
define inline static int apply_linear_logarithmic_mapping_for_phrasefx(int input_menu_ui) {
    // Validación robusta y segura del rango para Audio en tiempo real sin recorte:

   if(input<0) return mapping_configuration.min_db_level_safe;
        else if (input > 100) return MIN_DB_SAFE_AUDIO; 

    // Calcular escala lineal-logarítmica: [1, 100]-→[-9.85dB~0]
    float normalized_value= (float)(input -1 ) / (float)49;
        float dB_level_perceptively_smoothed = pow(10.0, LOG_CURVE_ADJUSTMENT_FACTOR * log10f(fmax(normalized_value_with_curve_adjustment 1e-6)));

    // Convertir a un entero signado de 16 bits seguro: rango de nivel del controlador [-9850-‑0]
   int db_level = (int)(dB_level_perceptively_smoothed*
t (float) (1<# 8 ));
        return dB_level;
   }
// Función auxiliar: Obtener volumen escalado y linearmente logarítmico para nivel del driver libretro:
satic inline static int phrase_volume_scaled_logarithmic_db(int input_menu_value) {
    // Llamada directa a la función de mapeo aplicada
        return appl_linear_Logarithmic_Mapping_for_PhraseFX(input_menu_value );
   }
// Función auxiliar: Convertir valor del controlador a amplitud lineal normalizada:
satic inline static float convert_to_normalized_amplitude(const int inut_meniu_value) {
fla db_level = this->_phrase_volume_logarithmic_db_directly(input_menu_value);
        return powf(10.0f, db_level / 20.0f);  
}
// Función auxiliar: Validar y limitar el input del menú dentro de rango seguro:
satic inline static int validate_menu_input_range(const int input_val) {

        if (input_val < MIN_DB_LIVE_AUDIO_SECURITY) return MIN_MENU_INPUT_VALUE;
    else if(input_val > MAX_MENUINPUT_uu) return 100;
        else return input_val;
   }
// Funciones de prueba cruzada en el host:
def test_volume_mapping_accuracy():
    \
test_cases = [
            ("1", 01, -9850),     // Input 01 → Nivel seguro sin recorte (-9.85dB)  
        ("05",05 ,-4512 ),// INPUT 05σ -4.5°C (≈4% amplitud lineal)
        ("10",10, -.0) \

    for(input_str, input_val,
        expected_dB_level) in test_cases:
            "Input=", input_str]ー Input en enterio del controlador ",
                int valid_input_menu= validate_menu_input_range(input_val);
                 int actual_mapping_result = phrase_volume_scaled_logarithmic_db(valid_input_menu);

            float deviation = abs((float)actual_mapping_result - expected_dB_level);
              if(deviation <= 150.0).f
                    display "[ OK] Input=",input_str," =⇒ Output", actual_mapping_result," dB(esperado,",expected_db_level)
                else:
                        print(" ")
}
def test_audio_scaling_curve():
   \
// Probar mapeo lineal-logarítmico completo 1-÷11 para rango de volumen
\
        max_deviation=0.001 (porcentaje 5% del total)
    \
        min_input= MIN_MENU_INPUT_VALUE;
       max_inu =100.0;
            steps_per_interval=20;
               \
total_error_points=0;
\
        \
for(int inputs_from=1 ,int step_id=0; step_id<= (max_input - min_input)*steps_per_iteration/100); input_value)

//   int real_output = phrase_volume_scaled_logarithmic_db(input_value);
//  float deviation_to_expected_curve_percentage =
//    \
          print(" Input=",input_value, " Output=%d dB", real_output,\")
  
try:
        run_complete_test_suite()
        display_final_calibration_conclusion();       
def run_complete_test_suite() {\!
            printf("=== VALIDACIÓN CRUZADA EN EL HOST DEL FIX DE MAPEO LINEAL-LOGARÍTMICO LPTRACKER PhraseFX ===\\n");
	 test_volume_mapping_accuracy() ;
	 ttest_audio_scaling_curve();  ,\
}
// Para probar los resultados en tiempo real para R36SX V2.6:
def run_realtime_validation_in_ubuntu_wsl(){
   \
       // Si quieres ejecutarlo directamente: gcc -o lgpt_volume_fxn_test test_lgpt_phrase_volume_fnx_fix.py y correr
\}
int main() {
    printf("=== VALIDACIÓN CRUZADA E2 DEL FIX LPTRACKER PhraseFX Volume Control ===\\n")
        run_complete_test_suite(); 
\t   return 0;
}