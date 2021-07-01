#!/usr/bin/python3

import pandas as pd
import xlsxwriter
import csv
import matplotlib.pyplot as plt
import os.path
import numpy
from os import path
from io import BytesIO 
from pandas.plotting import table 
from scipy import stats

def get_count_sum_df(df, memory_func):
    sum_column = df.iloc[:, 1:].sum(axis=0)
    sum_column = sum_column.to_frame()
    sum_column.columns = [memory_func+'_count']
    return sum_column

def get_avg_latency_df(latency_df, count_df, memory_func):
    tmp_latecny_df = latency_df.iloc[:, 1:]
    tmp_count_df = count_df.iloc[:, 1:]
    sum_column = tmp_count_df.sum(axis=0)
    sum_column = sum_column.to_frame()
    sum_column.columns = ['count']
    
    # print("*********************sum_column")
    # print(sum_column)

    tmp_df = tmp_count_df.mul(tmp_latecny_df)
    # print("**************tmp_df:")
    # print(tmp_df)
    sum_latency = tmp_df.sum(axis=0)
    sum_latency = sum_latency.to_frame()
    sum_latency.columns = ['count']
    # print("**************sum_latency:")
    # print(sum_latency)
    avg_latency_df = sum_latency.div(sum_column)
    # print("**************avg_latency_df")
    # print(avg_latency_df)
    avg_latency_df.columns = [memory_func + 'avg. latency']
    avg_latency_df = avg_latency_df[avg_latency_df[memory_func + 'avg. latency'].notna()]
    # avg_latency_df = avg_latency_df[(avg_latency_df.T != 0).any()]
    
    # print(avg_latency_df)    
    return avg_latency_df

def process_trace_file(pdwriter, file_path):
    file = file_path + "/function_trace.csv"
    # pdwriter = pd.ExcelWriter("/home/postgres/mstatis/build_log/trace_report.xlsx", engine='xlsxwriter')
    # fin = open(file, "rt")
    # data = fin.read()
    # data = data.replace("<=1 ","<=1")
    # data = data.replace("<=2 ","<=2")
    # data = data.replace("<=3 ","<=3")
    # data = data.replace("<=4 ","<=4")
    # data = data.replace("<=5 ","<=5")
    # data = data.replace("<=5 ","<=5")
    # data = data.replace("Regi1 PostmasterMain","Regi1_PostmasterMain")
    # data = data.replace("1EmitErrorR ","1EmitErrorR")
    # fin.close()
    # fin=open(file, "wt")
    # fin.write(data)
    # fin.close()

    trace_df = pd.read_csv(file, sep=',', error_bad_lines=False)
    # print(trace_df.head())
    grouped_trace_df = trace_df.groupby(['function']).sum()
    grouped_trace_df.to_excel(pdwriter, sheet_name="sheet1")

    column_names = ["1-64", "65-128", "129-256", "257-512", "513-1K", "1K-2K", "2K-4K","4K-8K", "8K-16K", "16K-32K", "32K-64K", "128K-256K", "256K-512K", "512K-1M", "1M-2M", "2M-4M", ">4M"]
    # column_names = ["1-64", "65-128", "129-256", "257-512", "513-1K", "1K-2K", "2K-4K","4K-8K", "8K-16K", "16K-32K", "32K-64K", "128K-256K", "256K-512K", "512K-1M", "1K-2M", "2K-4M", ">4M"]

    i = 0
    for column in reversed(column_names):
        slice_df = grouped_trace_df[[column]]
        slice_df = slice_df.nlargest(10, column).head(10)
        df_row_len = slice_df.shape[0]
        slice_df.to_excel(pdwriter, sheet_name="sheet2", startrow = (df_row_len + 2)*i )
        i = i + 1

    # _4M_trace_df = grouped_trace_df[[">4M"]]
    # _4M_trace_df = _4M_trace_df.nlargest(10, ">4M").head(10)
    # _4M_trace_df.to_excel(pdwriter, sheet_name="sheet2")

    # df_row_len = _4M_trace_df.shape[0]

    # print(grouped_trace_df.head())
    # _2M_4M_trace_df = grouped_trace_df[["2M-4M"]]
    # _2M_4M_trace_df = _2M_4M_trace_df.nlargest(10, "2M-4M").head(10)
    # _2M_4M_trace_df.to_excel(pdwriter, sheet_name="sheet2", startrow = df_row_len + 2)

def process_file(pdwriter, file_path, chart_sheet):
    supported_funcs = ["memset", "memmove", "memcpy"]

    sum_count_df = pd.DataFrame()

    avg_latency_df = pd.DataFrame()
    avg_latency_df.to_excel(pdwriter, sheet_name="summary")
    df_row_len = 0
    for func in supported_funcs:
        # print("func:" + func)
        interval_file = file_path + "/" + func +"_interval.csv"
        latency_file = file_path + "/" + func +"_latency.csv"

        count_df = pd.read_csv(interval_file, sep=',')
        lantecy_df = pd.read_csv(latency_file, sep=',')

        # remove outlier data
        tmp_latecny_df = lantecy_df.iloc[:, 1:]
        tmp_latecny_df = tmp_latecny_df.mask((tmp_latecny_df - tmp_latecny_df.mean()).abs() > 2 * tmp_latecny_df.std())
        is_NaN = tmp_latecny_df.isnull()
        row_has_NaN = is_NaN.any(axis=1)
        true_count = sum(row_has_NaN)
        print(true_count)
        lantecy_df = lantecy_df[~numpy.array(row_has_NaN)]
        count_df = count_df[~numpy.array(row_has_NaN)]


        count_df.to_excel(pdwriter, sheet_name=func+"_count")        
        sum_count_df = pd.concat([sum_count_df, get_count_sum_df(count_df, func)], axis = 1)
        
        lantecy_df.to_excel(pdwriter, sheet_name=func+"_latency")
    
        

        df_row_len = lantecy_df.shape[0]
        avg_latency_df = pd.concat([avg_latency_df, get_avg_latency_df(lantecy_df, count_df, func)], axis = 1)

    workbook = pdwriter.book
    summary_sheet = workbook.get_worksheet_by_name("summary")
    # print(sum_count_df)
    summary_sheet.write(1, 0, "count summary")
    sum_count_df_startrow = 2
    sum_count_df = sum_count_df[(sum_count_df.T != 0).any()]
    sum_count_df.to_excel(pdwriter, sheet_name="summary", startrow=sum_count_df_startrow)
    sum_count_df_rowlen = sum_count_df.shape[0]
    
    # print("avg_latency_df:")
    # print(avg_latency_df)
    summary_sheet.write(sum_count_df_rowlen + 5, 0, "latency summary")
    avg_latency_df_startrow = sum_count_df_rowlen + 6
    avg_latency_df = avg_latency_df[(avg_latency_df.T != 0).any()]
    avg_latency_df.to_excel(pdwriter, sheet_name="summary", startrow=avg_latency_df_startrow)
    avg_latency_df_rowlen = avg_latency_df.shape[0]

    value_col_index = 1
    index = 1
    for func in supported_funcs:
        bar_chart=workbook.add_chart({'type': 'bar'})
        bar_chart.add_series({
            'name':       '= count',
            'categories': ["summary", sum_count_df_startrow+1,0, sum_count_df_startrow +1 + sum_count_df_rowlen,0],
            'values':     ["summary", sum_count_df_startrow+1,value_col_index,sum_count_df_startrow + 1 + sum_count_df_rowlen,value_col_index],
            'data_labels': {'value': True},
        })        
        bar_chart.set_title({'name': func+" count"})
        bar_chart.set_size({'x_scale': 1.2, 'y_scale': 1.5})
        chart_sheet.insert_chart((index - 1) * 25 + 1, 1, bar_chart)

        pie_chart=workbook.add_chart({'type': 'pie'})
        pie_chart.add_series({
            'name':       '= count (%)',
            'categories': ["summary", sum_count_df_startrow+1,0, sum_count_df_startrow +1 + sum_count_df_rowlen,0],
            'values':     ["summary", sum_count_df_startrow+1,value_col_index,sum_count_df_startrow + 1 + sum_count_df_rowlen,value_col_index],
            'data_labels': {'percentage': True, 'leader_lines': True},
        })        
        pie_chart.set_title({'name': func+" count"})
        pie_chart.set_size({'x_scale': 1.2, 'y_scale': 1.5})
        chart_sheet.insert_chart((index - 1) * 25 + 1, 12, pie_chart)

        latency_col_chart=workbook.add_chart({'type': 'bar'})
        latency_col_chart.add_series({
            'name':       '= latency (us)',
            'categories': ["summary", avg_latency_df_startrow+1,0, avg_latency_df_startrow +1 + avg_latency_df_rowlen,0],
            'values':     ["summary", avg_latency_df_startrow+1,value_col_index,avg_latency_df_startrow + 1 + avg_latency_df_rowlen,value_col_index],
            'data_labels': {'value': True},
        })        
        latency_col_chart.set_title({'name': func+" avg. latency"})
        latency_col_chart.set_size({'x_scale': 1.2, 'y_scale': 1.5})
        chart_sheet.insert_chart((index - 1) * 25 + 1, 24, latency_col_chart)

        # print("df_row_len: " + str(df_row_len))

        # latency_col_perc_chart=workbook.add_chart({'type': 'bar', 'subtype': 'percent_stacked'})
        # # for col_num in range (2, 18):
        # #     latency_col_perc_chart.add_series({
        # #         'name':       [func+"_latency", 0, col_num],
        # #         'categories': [func+"_latency", 0, 2, 0, 18],
        # #         'values':     [func+"_latency", 1, col_num, df_row_len, col_num],
        # #         'data_labels': {'value': True},
        # #     })

        # for row_num in range (1, df_row_len):
        #     latency_col_perc_chart.add_series({
        #         'categories': [func+"_latency", 0, 2, 0, 18],
        #         'values':     [func+"_latency", row_num, 2, row_num, 18],
        #         'data_labels': {'value': True},
        #     })               
        # latency_col_perc_chart.set_title({'name': func+" latency"})
        # latency_col_perc_chart.set_size({'x_scale': 1, 'y_scale': 1.2})
        # chart_sheet.insert_chart((index - 1) * 20 + 1, 30, latency_col_perc_chart)                          


        value_col_index = value_col_index + 1
        index = index + 1
  

input_dir=input("Enter the data directory: ")
if not input_dir:
    input_dir = "./"
report_file=input_dir + "/report.xlsx"
print("The report file will be located at: " + report_file)

pdwriter = pd.ExcelWriter(report_file, engine='xlsxwriter') # pylint: disable=abstract-class-instantiated
workbook  = pdwriter.book
chart_sheet = workbook.add_worksheet("Sheet1")
process_file(pdwriter, input_dir, chart_sheet)
pdwriter.save()

report_file=input_dir + "/report_func.xlsx"
pdwriter = pd.ExcelWriter(report_file, engine='xlsxwriter') 
process_trace_file(pdwriter, input_dir, )
pdwriter.save()