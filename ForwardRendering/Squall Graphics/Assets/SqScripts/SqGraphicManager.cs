using System;
using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// squall graphic manager
/// </summary>
public class SqGraphicManager : MonoBehaviour
{
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

    /// <summary>
    /// number of render threads
    /// </summary>
    [Range(2, 32)]
    public int numOfRenderThreads = 4;

    /// <summary>
    /// global aniso level
    /// </summary>
    [Range(1, 16)]
    public int globalAnisoLevel = 8;

    /// <summary>
    /// reseting frame
    /// </summary>
    [HideInInspector]
    public bool resetingFrame = false;

    /// <summary>
    /// instance
    /// </summary>
    public static SqGraphicManager Instance { private set; get; }

    void Awake()
    {
        if (Instance != null)
        {
            Debug.LogError("Only one SqGraphicManager can be created.");
            enabled = false;
            return;
        }

        if (numOfRenderThreads < 2)
        {
            numOfRenderThreads = 2;
        }

        if (InitializeSqGraphic(numOfRenderThreads))
        {
            Debug.Log("[SqGraphicManager] Squall Graphics initialized.");
            Instance = this;
        }
        else
        {
            Debug.LogError("[SqGraphicManager] Squall Graphics terminated.");
            enabled = false;
            return;
        }

        Debug.Log("[SqGraphicManager] Number of render threads: " + GetRenderThreadCount());
    }

    void OnApplicationQuit()
    {
        ReleaseSqGraphic();
    }

    void LateUpdate()
    {
        if (resetingFrame)
        {
            resetingFrame = false;
            return;
        }

        UpdateSqGraphic();
        RenderSqGraphic();
    }
}
