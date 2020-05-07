﻿using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// squall graphic manager
/// </summary>
public class SqGraphicManager : MonoBehaviour
{
    struct GameTime
    {
        /// <summary>
        /// update time
        /// </summary>
        public double updateTime;

        /// <summary>
        /// render time
        /// </summary>
        public double renderTime;

        /// <summary>
        /// render thread time
        /// </summary>
        public double renderThreadTime;

        /// <summary>
        /// gpu time
        /// </summary>
        public double gpuTime;
    };

    [DllImport("SquallGraphics")]
    static extern bool InitializeSqGraphic(int _numOfThreads);

    [DllImport("SquallGraphics")]
    static extern void ReleaseSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void UpdateSqGraphic();

    [DllImport("SquallGraphics")]
    static extern void RenderSqGraphic();

    [DllImport("SquallGraphics")]
    static extern int GetRenderThreadCount();

    [DllImport("SquallGraphics")]
    static extern GameTime GetGameTime();

    /// <summary>
    /// number of render threads
    /// </summary>
    [Range(1, 32)]
    public int numOfRenderThreads = 4;

    /// <summary>
    /// print timer info
    /// </summary>
    public bool printTimerInfo = false;

    GameTime gameTime;
    float gameTimeUpdate = 0f;
    GUIStyle guiStyle;

    void Awake()
    {
        if (InitializeSqGraphic(numOfRenderThreads))
        {
            Debug.Log("[SqGraphicManager] Squall Graphics initialized.");
        }
        else
        {
            Debug.LogError("[SqGraphicManager] Squall Graphics terminated.");
            enabled = false;
            return;
        }

        Debug.Log("[SqGraphicManager] Number of render threads: " + GetRenderThreadCount());
        guiStyle = new GUIStyle();
        guiStyle.fontSize = 24 * Screen.width / 1920;
        guiStyle.normal.textColor = Color.white;
    }

    void OnDestroy()
    {
        ReleaseSqGraphic();
    }

    void LateUpdate()
    {
        UpdateSqGraphic();
        RenderSqGraphic();
        gameTimeUpdate += Time.deltaTime;
    }

#if DEBUG
    void OnGUI()
    {
        if (printTimerInfo)
        {
            if (gameTimeUpdate > 1f)
            {
                gameTime = GetGameTime();
                gameTimeUpdate = 0f;
            }

            GUI.Label(new Rect(10, 10, 500, 20 * Screen.width / 1920), "Update() Call Time: " + gameTime.updateTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 20 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "Render() Call Time: " + gameTime.renderTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 40 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "RenderThread Time: " + gameTime.renderThreadTime.ToString("0.#####") + " ms", guiStyle);
            GUI.Label(new Rect(10, 10 + 60 * Screen.width / 1920, 500, 20 * Screen.width / 1920), "GPU Time: " + gameTime.gpuTime.ToString("0.#####") + " ms", guiStyle);
        }
    }
#endif
}