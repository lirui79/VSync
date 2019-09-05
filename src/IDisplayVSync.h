/*****************************************************/
/*
**   Author: lirui
**   Date: 2019/06/14
**   File: IDisplayVSync.h
**   Function:  Interface of IDisplayVSync for user
**   History:
**       2019/06/14 create by lirui
**
**   Copy Right: iris corp
**
*/
/*****************************************************/
#ifndef VIRTUAL_INTERFACE_DISPLAY_VSYNC_H
#define VIRTUAL_INTERFACE_DISPLAY_VSYNC_H

/**
 * @addtogroup Platform
 * @{
 */

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct IDisplayVSync {
	/**
	 * @brief IDisplayVSync 销毁函数
	 *
	 * @param[in]  vc     结构体指针
	 *
	 * @return 返回说明
	 *        -<em> < 0</em>   失败
	 *        -<em> >=0</em>   成功
	 *
	 * @example struct IDisplayVSync* vc = allocateIDisplayVSync();
	 *          ...
	 *          int code = vc->destroy(vc);
	 */
	int (*destroy)(struct IDisplayVSync* vc);

	/**
	 * @brief IDisplayVSync 初始化启动VSync同步函数
	 *
	 * @param[in]  vc     结构体指针
	 *
	 * @return 返回说明
	 *        -<em> < 0</em>   失败
	 *        -<em> >=0</em>   成功
	 *
	 * @example struct IDisplayVSync* vc = allocateIDisplayVSync();
	 *          ...
	 *          int code = vc->init(vc);
	 */
	int (*init)(struct IDisplayVSync* vc);

	/**
	 * @brief IDisplayDevice 退出VSync同步函数
	 *
	 * @param[in]  vc     结构体指针
	 *
	 * @return 返回说明
	 *        -<em> < 0</em>   失败
	 *        -<em> >=0</em>   成功
	 *
	 * @example struct IDisplayVSync* vc = allocateIDisplayVSync();
	 *          int code = vc->init(vc);
	 *          code = vc->exit(vc);
	 */
	int (*exit)(struct IDisplayVSync* vc);

	/**
	 * @brief IDisplayDevice VSync等待函数
	 *
	 * @param[in]  vc     结构体指针
	 * @param[in]  phase  等待时间纳秒
	 *
	 * @return 返回说明
	 *        -<em> < 0</em>   失败
	 *        -<em> >=0</em>   成功
	 *
	 * @example struct IDisplayVSync* vc = allocateIDisplayVSync();
	 *          int code = vc->init(vc);
	 *          code = vc->wait(vc, 7500000);
	 */
	int (*wait)(struct IDisplayVSync* vc, int64_t phase);

	/**
	 * @brief IDisplayDevice VSync预期时间相位剩余函数
	 *
	 * @param[in]  vc     结构体指针
	 * @param[in]  phase  预期时间相位纳秒
	 *
	 * @return 返回说明
	 *        -<em> < 0</em>   失败
	 *        -<em> >=0</em>   成功 剩余时间相位 纳秒
	 *
	 * @example struct IDisplayVSync* vc = allocateIDisplayVSync();
	 *          int code = vc->init(vc);
	 *          int64_t rtime = vc->remainingTime(vc, 13500000);
	 */
	int64_t (*remainingTime)(struct IDisplayVSync* vc, int64_t phase);
} IDisplayVSync;

/**
 * @brief 分配IDisplayVSync结构体指针函数
 *
 * @return 返回说明
 *        -<em>非NULL</em>    成功 返回结构体指针
 *        -<em>NULL</em>      失败
 *
 * @example
 *  struct IDisplayVSync *vc = allocateIDisplayVSync();
 */
typedef struct IDisplayVSync* (*allocateIDisplayVSyncType)();
__attribute__((visibility("default"))) struct IDisplayVSync*
allocateIDisplayVSync();

#if defined(__cplusplus)
}
#endif

/**
 * @}
 */

#endif  // VIRTUAL_INTERFACE_DISPLAY_VSYNC_H
