#!/bin/bash


function main_processor {
	find_outcar_files
	print_list_outcar_files

	if [[ ${#all_files_paths[@]} > 0 ]]; then
		if [[ $list_only_flag == 0 ]]; then
			find_max_force_dialog
		fi
	fi
}


function find_max_force_dialog {
	if [[ ${#all_files_paths[@]} == 1 ]]; then
		echo -n "Process this file? [Y/n] "
		read proc_one_file_choice
		echo -e "\n"
		if [ -z $proc_one_file_choice] || [[ $proc_one_file_choice == "y" ]] || [[ $proc_one_file_choice == "Y" ]]; then
			print_max_force_from_one_file ${all_files_paths[0]} $print_table_flag
		fi
	else
		echo -n "Enter numbers of process files: "
		read choice_array
		choice_array=($(echo "$choice_array" | grep -Eo "[0-9\-]+"))

		echo -e "Start process files...\n"
		for j in ${choice_array[@]}
		do
			temp=($(echo "$j" | grep -Eo "[0-9]+"))
			if [[ ${#temp[@]} > 1 ]]; then
				for (( q = ${temp[0]}; q <= ${temp[1]}; q++ ))
				do
					echo "File: \"${all_files_paths[$q-1]}\""
					print_max_force_from_one_file ${all_files_paths[$q-1]} $print_table_flag
				done
			else
				echo "File: \"${all_files_paths[$temp-1]}\""
				print_max_force_from_one_file ${all_files_paths[$temp-1]} $print_table_flag
			fi	
		done
	fi
}


function find_outcar_files {
	echo -e "\nFind files into directory \"$path_to_main_directory\"\n"
	all_files_paths=($(find $path_to_main_directory -name OUTCAR))
}


function print_list_outcar_files {
	if [ -z $all_files_paths ]; then
		echo "Cannot find OUTCAR files!"
	elif [[ ${#all_files_paths[@]} == 1 ]]; then
		echo "Only one file was founded"
		echo -e "\t${all_files_paths[0]}"
	else
		i=1
		echo -e "Found files:"
		for j in ${all_files_paths[@]}
		do
			echo -e "\t$i $j"
			let "i=i+1"
		done
	fi
	echo -e "\n"
}


function print_max_force_from_one_file {
	# Forming boundaries

	boundaries=$(cat $1 | grep -En "TOTAL-FORCE|total drift" | tail -n 2 | awk '{print $1}' | grep -Eo "[0-9]+")

	hight_line_num=$(echo "$boundaries" | awk "NR == 1")
	let "hight_line_num=hight_line_num+2"

	low_line_num=$(echo "$boundaries" | awk "NR == 2")
	let "low_line_num=low_line_num-2"


	# Getting table

	num_table=$(sed -n "${hight_line_num},${low_line_num}p" $1)


	# Print table if argument pass

	if [[ $2 == 1 ]]; then
		echo "The result table:";

		echo "---------------------------------------------------------------------------------------";

		echo "$num_table";

		echo "---------------------------------------------------------------------------------------";
	fi


	# Getting columns from table

	first_column=$(echo "$num_table" | awk '{print $4}')
	second_column=$(echo "$num_table" | awk '{print $5}')
	third_column=$(echo "$num_table" | awk '{print $6}')


	# Calculate the maximum element in each of the columns

	max_num_in_first_column=$(echo "$first_column" | awk 'n < $0 {n=$0}END{print n}')
	max_num_in_second_column=$(echo "$second_column" | awk 'n < $0 {n=$0}END{print n}')
	max_num_in_third_column=$(echo "$third_column" | awk 'n < $0 {n=$0}END{print n}')


	# Print max elements

	echo "1 column max: $max_num_in_first_column"
	echo "2 column max: $max_num_in_second_column"
	echo "3 column max: $max_num_in_third_column"
	echo -e "\n"
}


list_only_flag=0
print_table_flag=0
path_to_main_directory="./"
all_files_paths=()

while [[ $# -gt 0 ]]
do
	key=$1
	
	case $key in
		t|-t|--table)
			print_table_flag=1
			;;
		l|-l|--list)
			list_only_flag=1
			;;
		*)
			path_to_main_directory=$key
			;;
	esac

	shift
done

main_processor

echo "Exiting..."

