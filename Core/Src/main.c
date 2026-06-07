/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - ALCOSONDE
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ili9341.h"
#include "fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define F_CLK     32000000.0f  // Horloge du timer TIM2 [Hz]
#define K         4.55e-7f     // Constante du conditionneur : f = K / C  [F.Hz]
#define CP        8.55f         // Capacite parasite du PCB [pF]
#define PENTE     0.135f      // Pente COMSOL : C_sonde[pF] = PENTE * eps_r
#define EPS_EAU   80.1f        // Permittivite a 0%vol (eau pure, Akerlof)
#define PENTE_ALC 0.76f        // eps_r diminue de 0.76 par %vol (Akerlof, 0-40%)
#define NB_PLAGES 8            // Nombre de plages d'alcool selectionnables
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi3;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint32_t ic_val1 = 0;        // capture precedente
volatile uint32_t ic_val2 = 0;        // capture courante
volatile uint32_t periode_ticks = 0;  // periode mesuree, en ticks d'horloge
char buffer[80];                      // chaine pour le port serie

// --- Plages d'alcool selectionnables ---
uint8_t plage = 0;                                              // plage cible actuelle (0..NB_PLAGES-1)
const uint8_t plage_min[NB_PLAGES] = {0, 5, 10, 15, 20, 25, 30, 35};
const uint8_t plage_max[NB_PLAGES] = {5, 10, 15, 20, 25, 30, 35, 40};
uint32_t t_dernier_appui = 0;         // pour l'anti-rebond du bouton
uint8_t  bouton_precedent = 1;        // etat precedent du bouton (1 = relache)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
		HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);

  // Reset manuel franc de l'ecran
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_Delay(100);

  ILI9341_Init();
  ILI9341_FillScreen(ILI9341_BLACK);

  // Decor fixe (dessine une seule fois)
  ILI9341_WriteString(50, 15,  "ALCOSONDE", Font_16x26, ILI9341_CYAN,  ILI9341_BLACK);
  ILI9341_WriteString(10, 80,  "Cible:",    Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
  ILI9341_WriteString(10, 140, "Mesure:",   Font_11x18, ILI9341_WHITE, ILI9341_BLACK);

  // --- Buzzer sur PA8 ---
  GPIO_InitTypeDef GPIO_Buzzer = {0};
  GPIO_Buzzer.Pin = GPIO_PIN_8;
  GPIO_Buzzer.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_Buzzer.Pull = GPIO_NOPULL;
  GPIO_Buzzer.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_Buzzer);

  // --- Melodie d'accueil (buzzer actif : 1 note, on joue sur le rythme) ---
  uint16_t melodie[]  = {80, 80, 80, 80, 250};
  uint16_t silences[] = {60, 60, 60, 120, 0};
  for (int i = 0; i < 5; i++)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(melodie[i]);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(silences[i]);
  }

  // --- Bouton poussoir sur PB0 (entree avec pull-up interne) ---
  GPIO_InitTypeDef GPIO_Bouton = {0};
  GPIO_Bouton.Pin = GPIO_PIN_0;
  GPIO_Bouton.Mode = GPIO_MODE_INPUT;
  GPIO_Bouton.Pull = GPIO_PULLUP;
  GPIO_Bouton.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_Bouton);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

      // ===== 1. BOUTON : passer a la plage suivante (anti-rebond 200 ms) =====
      uint8_t bouton = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
      if (bouton_precedent == 1 && bouton == 0 && (HAL_GetTick() - t_dernier_appui > 200))
      {
        plage = (plage + 1) % NB_PLAGES;     // plage suivante (revient a 0 apres la derniere)
        t_dernier_appui = HAL_GetTick();
      }
      bouton_precedent = bouton;

      // ===== 2. AFFICHER la plage cible (seulement si elle a change) =====
      static uint8_t plage_prec = 255;
      if (plage != plage_prec)
      {
        char txt_cible[16];
        sprintf(txt_cible, "%u-%u%%   ", plage_min[plage], plage_max[plage]);
        ILI9341_WriteString(110, 76, txt_cible, Font_16x26, ILI9341_YELLOW, ILI9341_BLACK);
        plage_prec = plage;
      }

      // ===== 3. MESURE -> titre alcoometrique =====
      if (periode_ticks > 0)
      {
        float f = F_CLK / (float)periode_ticks;   // frequence [Hz]
        float C_tot = (K / f) * 1e12f;            // capacite totale [pF]
        float C_sonde = C_tot - CP;               // capacite sonde seule [pF]
        float eps_r = C_sonde / PENTE;            // permittivite relative
        float titre = (EPS_EAU - eps_r) / PENTE_ALC;   // titre [%vol] (loi Akerlof)
        if (titre < 0.0f)  titre = 0.0f;
        if (titre > 99.0f) titre = 99.0f;
        uint32_t titre_aff = (uint32_t)titre;

        // afficher la mesure (seulement si elle a change, pour eviter le clignotement)
        static uint32_t titre_prec = 9999;
        if (titre_aff != titre_prec)
        {
          char txt_mes[16];
          sprintf(txt_mes, "%lu%%   ", titre_aff);
          ILI9341_WriteString(120, 136, txt_mes, Font_16x26, ILI9341_WHITE, ILI9341_BLACK);
          titre_prec = titre_aff;
        }

        // dans la cible ? (etat memorise pour ne biper qu'une fois)
        static int8_t etat_prec = -1;
        if (titre >= (float)plage_min[plage] && titre < (float)plage_max[plage])
        {
          if (etat_prec != 1)   // on vient juste d'entrer dans la cible
          {
            ILI9341_WriteString(10, 210, "DANS LA CIBLE", Font_16x26, ILI9341_GREEN, ILI9341_BLACK);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);   // bip de validation
            HAL_Delay(400);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
            etat_prec = 1;
          }
        }
        else
        {
          if (etat_prec != 0)
          {
            ILI9341_WriteString(10, 210, "hors cible   ", Font_16x26, ILI9341_RED, ILI9341_BLACK);
            etat_prec = 0;
          }
        }

        // log complet sur le terminal serie (pour la calibration)
                uint32_t f_aff   = (uint32_t)f;
                uint32_t C_x10   = (uint32_t)(C_tot * 10.0f);    // capacite x10 pour 1 decimale
                uint32_t eps_x10 = (uint32_t)(eps_r * 10.0f);    // eps_r x10 pour 1 decimale

                int len = sprintf(buffer,
                      "f=%lu Hz | C=%lu.%lu pF | eps_r=%lu.%lu | titre=%lu %% | cible %u-%u %%\r\n",
                      f_aff, C_x10/10, C_x10%10, eps_x10/10, eps_x10%10,
                      titre_aff, plage_min[plage], plage_max[plage]);
                HAL_UART_Transmit(&huart2, (uint8_t*)buffer, len, 100);
      }

      HAL_Delay(120);   // ~8 lectures/seconde (assez reactif pour le bouton)
    /* USER CODE END 3 */
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI3 Initialization Function
  */
static void MX_SPI3_Init(void)
{
  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */
}

/**
  * @brief TIM2 Initialization Function
  */
static void MX_TIM2_Init(void)
{
  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
  * @brief USART2 Initialization Function
  */
static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TFT_CS_Pin|TFT_RST_Pin|TFT_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TFT_CS_Pin TFT_RST_Pin TFT_DC_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin|TFT_RST_Pin|TFT_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    ic_val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    periode_ticks = ic_val2 - ic_val1;  // soustraction non signee : gere le debordement 32 bits
    ic_val1 = ic_val2;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
