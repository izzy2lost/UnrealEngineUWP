import { createTheme } from '@fluentui/react';
import { HordeTheme, HordeThemeExtensions } from './themeTypes';

const baseWhite = "#D8D8D8"

// text colors
const textColor = baseWhite;
const linkColor = "#55B7FF";
const linkColorHovered = "#AADBFF";
const hilightColor = "#212425";

// Horde specific color extensions
const hordeDarkTheme: HordeThemeExtensions = {
    darkTheme: true,
    // Top nav background color
    topNavBackground: "#242729",
    // Breadcrumb area background color
    breadCrumbsBackground: "#1B1D1E",
    // Panel and modal content background color
    contentBackground: "#181A1B",
    // Neutral background color, used for site margins, and hinting such as in detail list rows
    neutralBackground: "#101010",
    // divides sections
    dividerColor: "#25282A",
    // Scrollbar theme colors
    scrollbarThumbColor: "#5F5F5F",
    scrollbarTrackColor: "#2F2F2F"
}

// Fluent theme
export const darkTheme = createTheme({
    isInverted: true,
    components: {
        "ScrollablePane": {
            styles: {
                root: {
                    selectors: {
                        '.ms-DetailsHeader': { // this is for stickys
                            background: hordeDarkTheme.contentBackground,
                            borderBottomColor: "#363A3C"
                        }
                    }
                }
            }
        },
        "DetailsList": {
            styles: {
                root: {
                    selectors: {
                        '.ms-DetailsHeader': {
                            background: hordeDarkTheme.contentBackground,
                            borderBottomColor: "#363A3C"
                        },
                        '.ms-DetailsRow:hover': {
                            backgroundColor: "unset",
                            background: hilightColor
                        }
                    }
                }
            }
        },
        "Modal": {
            styles: {
                main: {
                    background: hordeDarkTheme.contentBackground,
                }
            }
        },
        "Stack": {
            styles: {
                root: {
                    selectors: {
                        'a': {
                            color: linkColor
                        },
                        'a:hover': {
                            color: linkColorHovered
                        }
                    }
                }
            }
        },
        "Checkbox": {
            styles: {
                checkmark: {
                    color: "#FFFFFF"
                },
                checkbox: {
                    borderBottomColor: "#959595",
                    borderTopColor: "#959595",
                    borderLeftColor: "#959595",
                    borderRightColor: "#959595"
                }
            }
        },
        "Toggle": {
            styles: {
                root: {
                    selectors: {
                        '.ms-Toggle-thumb': {
                            background: "#FFFFFF !important"
                        }
                    }
                }
            }
        },
        "DatePicker": {
            styles: {
                callout: {
                    selectors: {
                        'button': {
                            color: "#FFFFFF !important",
                        },
                        'button:disabled': {
                            color: `#5F5F5F !important`,
                        },
                    }
                }
            }
        }
    },
    palette: {
        themePrimary: '#0078d4',
        themeLighterAlt: '#eff6fc',
        themeLighter: '#deecf9',
        themeLight: '#c7e0f4',
        themeTertiary: '#71afe5',
        themeSecondary: '#2b88d8',
        themeDarkAlt: '#106ebe',
        themeDark: '#005a9e',
        themeDarker: '#004578',
        neutralLighterAlt: '#212425',
        neutralLighter: '#212425',
        neutralLight: '#212425',
        neutralQuaternaryAlt: '#212425',
        neutralQuaternary: '#212425',
        neutralTertiaryAlt: '#212425',
        neutralTertiary: '#c8c8c8',
        neutralSecondary: '#d0d0d0',
        neutralPrimaryAlt: '#dadada',
        neutralPrimary: '#B5B5B5',
        neutralDark: '#f4f4f4',
        black: '#f8f8f8',
        white: '#181A1B',
    },
    semanticColors: {
        bodyBackground: hordeDarkTheme.neutralBackground,
        bodyText: textColor,
        buttonText: "#FFFFFF",
        buttonTextHovered: "#FFFFFF",
        buttonTextDisabled: "#949898",
        actionLink: textColor,
        link: linkColor,
        linkHovered: linkColorHovered,
        menuHeader: "#55B7FF",
        menuItemText: textColor,
        menuItemTextHovered: "#F1F1F1",
        menuItemBackgroundHovered: hilightColor,
        listText: textColor,
        primaryButtonText: "#FFFFFF",
        primaryButtonTextHovered: "#FFFFFF",
        primaryButtonTextDisabled: "#949898",
        inputText: textColor,
        inputPlaceholderText: "#888888",
        inputBorder: "#959595",
        inputBorderHovered: "#B5B5B5",
        smallInputBorder: "#959595"

    },
    defaultFontStyle: {
        fontFamily: 'Horde Open Sans Regular'
    },
    fonts: {
        small: { fontSize: 12 },
        medium: { fontSize: 13 },
        mediumPlus: { fontSize: 18 },
        large: { fontSize: 32 }
    }
}) as HordeTheme;

darkTheme.horde = hordeDarkTheme;