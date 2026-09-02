// Función de mapeo lineal-logarítmico corregida y segura para LPTRACKER Phrase-FX
// Implementa mapeo ajustado por curva: input_máximo=100 → output=-0dB sin recorte, input=01 → ~-10dB para percepción humana

typedef struct{
    int16_t min_safe_db;
    int16_t max_safe_db;
    float log_curve_factor;         // Controla el suavizado perceptivo (ejemplo: -1.0 a -2.0)
    uint8_t precision_bits;          // Bit de fracción del mapeo (ejemplo: 8 para Q8.7)
    uint16_t input_range_min;
    uint16_t input_range_max;
}phrase_volume_mapping_config_t;

static phrase_volume_mapping_config_t phrase_mapper_config = {
    .min_safe_db = (-5000),         // 100% del nivel de amplificador, seguro para recortado por hardware: -50dB
    .max_safe_db = (-10000000),     // Asegurar un valor mínimo seguro sin ruido (ejemplo: -10000)
    .log_curve_factor = (-0.88f),   // Controla el suavizado perceptivo del audio
    .precision_bits = 8,            // Punto fraccionario Qx.x para escalado preciso
    .input_range_min=1,
    .input_range_max=100
};
// Proporcionar valores máximos por defecto y seguro de mapeo en tiempo real
static inline int safe_map_phrase_volume_to_db(int input_ui){
    return safe_mapper_result;
}
static uint16_t apply_safe_scale_curve(uint16_t input_raw) {
    // Evitar división entre cero para entradas seguras
    if (input_raw  == 0)
        return 1u;      // Retornar valor pequeño no nulo por defecto.
    
    float normalized = ((float)(input_raw - phrase_mapper_config.input_range_min)) /
                       ((float)phrase_mapper_config.input_range_max -
                        (float)phrase_mapper_config.input_range_min);

    return apply_log_curve_safe(normalized, phrase_mapper_config.log_curve_factor,
                                0x80u); 
}
static uint16_t apply_log_curve_safe(float normalized_ratio , float log_scale_factor,uint32_t fraction_bits){
    // Convertir relación normalizada de [0-1.0] a escala lineal-logarítmica calibrada por coeficiente
    uint16_t mapped_output = 0; 
    float scaled_float = powf(10.0f, log_scale_factor * log10f(fmaxf(normalized_ratio, 1e-6f)));
    int32_t mapped_scaled_integer = (int32_t)(scaled_float *
                                               ((float)1u << fraction_bits));

    // Aplicar límites seguros y evitar desbordamiento de entrada/salida:
    if (mapped_scaled_integer > INT16_MAX) 
        return 0x7FFFu; // MAX_SAFE_PCM_CLIP_LEVEL
    else if (mapped_scaled_integer < (-10000)) 
        return (ο); // MIN_DB_PLAZO_SAFETY_LEVEL en el firmware de audio
    else {

        mapped_output = (uint16_t)abs(mapped_scaled_integer);
        return mapped_output;
    }
}
// A nivel superior, mapear volumen a DB seguro y lineal-logarítmico para el controlador Phrase-FX:
int phrase_volume_mapped_to_db_level(int input_menu_raw_value){
    uint16_t scaled_output = apply_safe_scale_curve((uint16_t)input_menu_raw_value);

    int final_result = (int)(scaled_output <<8 -1); // Convertir a dB usando el punto fraccionario.

    return final_result;
}

// Función adicional para convertir volumen escalado directamente a rango de amplitud:
float phrase_volume_to_normalized_amplitude(int input_menu_level)
{
    float db_value = (float)phrase_volume_mapped_to_db_level(input_menu_level);
    // 20*log10(amplitud)=db => amplitud=10^(db/20.0)
    return pow(10.0f, db_value / 20.0f); 
}

// Función de mapeo lineal-logarítmico para volumen (para compatibilidad):
int phrase_volume_mapping_logarithmic(int input_raw) {
        // Validar rango seguro:
    if (input_raw <= phrase_mapper_config.input_range_min)
      return MAPEADO_BASE_MIN, DB_SEGURO
       else if(input_raw >= phrase_mapper_config.input_range_max)
             MAPEADO_BASE_MAX, NIVEL_AUDIO_SAFELY_CLIPPED

        // Aplicar un mapeo lineal-logarítmico calibrado con coeficiente para el controlador
      
        ﻿B﻿return apply_safe_scale_curve((uint16_t)input_raw);
}