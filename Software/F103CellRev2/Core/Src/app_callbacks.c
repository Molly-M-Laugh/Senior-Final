#include "main.h"
#include "power_ctrl.h"
#include "tmp75b_port.h"
#include "can_ctrl.h"


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
        case FLT_5V0_Pin:
        case FLT_3V3_Pin:
        case FLT_1V8_Pin:
            power_fault_irq_handler(GPIO_Pin);
            break;

        case INA3221_CRIT_Pin:
        case INA3221_WARN_Pin:
        case INA3221_VALID_Pin:
            /* TODO: ina3221_alert_handler(GPIO_Pin); */
            break;

        case PG5V0_Pin:
            /* TODO: optional PG drop handler */
            break;

        case TEMP0_ALERT_Pin:
        case TEMP1_ALERT_Pin:
        case TEMP2_ALERT_Pin:
        	tmp75b_alert_irq_handler(GPIO_Pin);
        	break;

        case CAN_ALRT1_Pin:
        case CAN_ALRT2_Pin:
        case CAN_FLT_Pin:
        	can_ctrl_irq_handler(GPIO_Pin);
        	break;

        case AN_ALRT1_Pin:
            /* TODO */
            break;

        default:
            break;
    }
}
